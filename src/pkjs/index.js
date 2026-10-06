console.log('=== TransLoc App JavaScript loaded ===');

var allAgencies = [];
var currentIndex = 0;
var currentApiKey = '';
var agenciesLoading = false;
var agenciesLoaded = false;
var pendingMessages = [];

/* Chunk must fit under the 1024-byte inbox opened in the C code, with
   headroom for the dictionary wrapper (keys, idx, total, entry headers).
   800 leaves ~200 bytes of margin. */
var CHUNK_SIZE = 800;
console.log('Chunk size: ' + CHUNK_SIZE);

function buildAgencyUrl(path) {
  var agency = allAgencies[currentIndex];
  if (!agency) {
    console.log('buildAgencyUrl: no current agency');
    return '';
  }
  if (!currentApiKey) {
    console.log('buildAgencyUrl: warning - API key not set');
  }
  return 'https://' + agency.domain + path +
         '?apiKey=' + encodeURIComponent(currentApiKey) +
         '&isPublicMap=true';
}

Pebble.addEventListener('appmessage', function(e) {
  console.log('AppMessage received: ' + JSON.stringify(e.payload));

  if (e.payload.PIN_KIND !== undefined) {
    handlePinRequest(e.payload);
    return;
  }

  // Location requests don't need the agency list.
  if (e.payload.FETCH_LOCATION !== undefined) {
    sendLocationToWatch();
    return;
  }

  var needsAgencies =
      (e.payload.FETCH_CONFIG !== undefined) ||
      (e.payload.FETCH_ROUTES !== undefined) ||
      (e.payload.FETCH_STOPS !== undefined) ||
      (e.payload.FETCH_STOP_ARRIVALS !== undefined) ||
      (e.payload.COMMAND !== undefined);

  if (needsAgencies && !agenciesLoaded) {
    console.log('Queuing (agencies not loaded): ' + JSON.stringify(e.payload));
    pendingMessages.push(e.payload);
    if (!agenciesLoading) fetchAgencyList();
    return;
  }

  processAppMessage(e.payload);
});

function processAppMessage(payload) {
  if (payload.FETCH_CONFIG !== undefined) {
    handleFetchConfig(payload.AGENCY_DOMAIN);
    return;
  }
  if (payload.FETCH_ROUTES !== undefined) {
    handleFetchRoutes();
    return;
  }
  if (payload.FETCH_STOPS !== undefined) {
    handleFetchStops(payload.ROUTE_ID);
    return;
  }
  if (payload.FETCH_STOP_ARRIVALS !== undefined) {
    handleFetchStopArrivals(payload.ROUTE_ID, payload.STOP_ID);
    return;
  }
  if (payload.COMMAND !== undefined) {
    handleCommand(payload.COMMAND, payload.LETTER, payload.JUMP_DOMAIN);
    return;
  }
  if (payload.REQUEST_AGENCIES !== undefined) {
    console.log('Initial request received (agencies already loaded)');
    return;
  }
}

// Ask the phone for a position; forward as ints scaled by 1e7.
function sendLocationToWatch() {
  if (!navigator.geolocation) {
    console.log('geolocation not available');
    Pebble.sendAppMessage({ 'LOCATION_ERROR': 1 });
    return;
  }
  console.log('Requesting location from phone...');
  navigator.geolocation.getCurrentPosition(
    function(pos) {
      var lat = Math.round(pos.coords.latitude  * 10000000);
      var lon = Math.round(pos.coords.longitude * 10000000);
      console.log('Location obtained: ' + lat + ',' + lon +
                  ' (accuracy ' + pos.coords.accuracy + 'm)');
      Pebble.sendAppMessage(
        { 'LOCATION_LAT': lat, 'LOCATION_LON': lon },
        function() { console.log('Location sent to watch'); },
        function(err) { console.log('Location send failed: ' + JSON.stringify(err)); }
      );
    },
    function(err) {
      console.log('geolocation error: code=' + err.code + ' msg=' + err.message);
      Pebble.sendAppMessage({ 'LOCATION_ERROR': err.code || 1 });
    },
    { enableHighAccuracy: true, timeout: 15000, maximumAge: 60000 }
  );
}

// Chunk payload into CHUNK_SIZE pieces; retries failed chunks.
function sendChunkedWithKeys(payload, idxKey, totalKey, dataKey, onDone) {
  var total = Math.ceil(payload.length / CHUNK_SIZE);
  if (total === 0) total = 1;

  var idx = 0;
  var maxRetries = 5;

  function sendNext(attempt) {
    if (attempt === undefined) attempt = 0;
    if (idx >= total) {
      if (onDone) onDone();
      return;
    }
    var chunk = payload.substring(idx * CHUNK_SIZE, (idx + 1) * CHUNK_SIZE);
    console.log('Sending chunk ' + (idx + 1) + '/' + total +
                ' (' + chunk.length + ' bytes)' +
                (attempt > 0 ? ' [retry ' + attempt + ']' : ''));

    var msg = {};
    msg[idxKey] = idx;
    msg[totalKey] = total;
    msg[dataKey] = chunk;

    Pebble.sendAppMessage(
      msg,
      function() {
        idx++;
        sendNext(0);
      },
      function(err) {
        if (attempt >= maxRetries) {
          console.log('Chunk ' + (idx + 1) + ' failed after ' +
                      maxRetries + ' retries: ' + JSON.stringify(err) +
                      ' — aborting transfer');
          return;
        }
        console.log('Chunk ' + (idx + 1) + ' failed: ' + JSON.stringify(err) +
                    ' — retrying (' + (attempt + 1) + '/' + maxRetries + ')');
        setTimeout(function() { sendNext(attempt + 1); }, 500);
      }
    );
  }

  sendNext(0);
}

function sendChunked(payload, onDone) {
  sendChunkedWithKeys(payload, 'CHUNK_IDX', 'CHUNK_TOTAL', 'CHUNK_DATA', onDone);
}

function sendStopChunks(payload, onDone) {
  sendChunkedWithKeys(payload, 'STOP_CHUNK_IDX', 'STOP_CHUNK_TOTAL',
                      'STOP_CHUNK_DATA', onDone);
}

function sendArrivalChunks(payload, onDone) {
  sendChunkedWithKeys(payload, 'ARRIVAL_CHUNK_IDX', 'ARRIVAL_CHUNK_TOTAL',
                      'ARRIVAL_CHUNK_DATA', onDone);
}

function handlePinRequest(payload) {
  var kind = payload.PIN_KIND;
  var isStop = (kind === 1);
  var eventTime = new Date(Date.now() + 5 * 60 * 1000).toISOString();
  var pinId = (isStop ? 'stop-' : 'depart-') + Date.now();

  var pin = {
    id: pinId,
    time: eventTime,
    layout: {
      type: 'genericPin',
      title: isStop ? 'Stop' : 'Depart',
      subtitle: 'In 5 minutes',
      body: isStop ? 'Next stop' : 'Next departure',
      tinyIcon: isStop ? 'system://images/NOTIFICATION_FLAG' : 'system://images/CAR_RENTAL'
    }
  };

  console.log('Inserting pin: ' + JSON.stringify(pin));
  Pebble.insertTimelinePin(pin);
}

function handleFetchConfig(domain) {
  if (allAgencies.length === 0) {
    console.log('No agencies, cannot fetch config');
    return;
  }

  if (domain) {
    for (var i = 0; i < allAgencies.length; i++) {
      if (allAgencies[i].domain === domain) {
        currentIndex = i;
        console.log('Restored currentIndex=' + i + ' from AGENCY_DOMAIN=' + domain);
        break;
      }
    }
  }

  var current = allAgencies[currentIndex];
  if (!current) return;

  var url = 'https://' + current.domain + '/Services/JSONPRelay.svc/GetMapConfig';
  console.log('Fetching config from: ' + url);

  var xhr = new XMLHttpRequest();
  xhr.open('GET', url);
  xhr.timeout = 15000;

  xhr.onload = function() {
    if (xhr.status === 200) {
      try {
        var config = JSON.parse(xhr.responseText);
        var apiKey = config.ApiKey || '';
        var colorStr = config.PrimaryColor ||
                       (config.MobileSettings && config.MobileSettings.PrimaryColor) ||
                       '';
        var colorVal = 0;
        if (colorStr) {
          var hex = String(colorStr).replace('#', '');
          if (/^[0-9a-fA-F]{6}$/.test(hex)) {
            colorVal = parseInt(hex, 16);
          }
        }

        currentApiKey = apiKey || '';
        console.log('Config: apiKey=' + currentApiKey + ' color=0x' + colorVal.toString(16));

        Pebble.sendAppMessage(
          { 'API_KEY': currentApiKey, 'PRIMARY_COLOR': colorVal },
          function() { console.log('Config sent'); },
          function(err) { console.log('Failed to send config: ' + JSON.stringify(err)); }
        );
      } catch (err) {
        console.log('Config parse error: ' + err.message);
        Pebble.sendAppMessage({ 'API_KEY': '', 'PRIMARY_COLOR': 0 });
      }
    } else {
      console.log('Config HTTP error: ' + xhr.status);
      Pebble.sendAppMessage({ 'API_KEY': '', 'PRIMARY_COLOR': 0 });
    }
  };

  xhr.onerror = function() {
    console.log('Config network error');
    Pebble.sendAppMessage({ 'API_KEY': '', 'PRIMARY_COLOR': 0 });
  };
  xhr.ontimeout = function() {
    console.log('Config timeout');
    Pebble.sendAppMessage({ 'API_KEY': '', 'PRIMARY_COLOR': 0 });
  };
  xhr.send();
}

// Routes payload: "P;<hex;hex;...>\n<name>;<idx>;<route_id>\n..."
function handleFetchRoutes() {
  if (allAgencies.length === 0) {
    console.log('No agencies, cannot fetch routes');
    sendChunked('P;\n', null);
    return;
  }
  if (!currentApiKey) {
    console.log('No API key yet — fetching config first');
    handleFetchConfig();
    return;
  }

  var routesUrl = buildAgencyUrl('/Services/JSONPRelay.svc/GetRoutes');
  console.log('Fetching routes from: ' + routesUrl);

  var rxhr = new XMLHttpRequest();
  rxhr.open('GET', routesUrl);
  rxhr.timeout = 15000;

  rxhr.onload = function() {
    if (rxhr.status === 200) {
      try {
        var routes = JSON.parse(rxhr.responseText);
        var filtered = [];
        if (Array.isArray(routes)) {
          for (var i = 0; i < routes.length; i++) {
            var r = routes[i];
            if (r.HideRouteLine === true) continue;
            if (r.IsVisibleOnMap === false) continue;
            var rname = r.Description || '';
            if (rname.length === 0) continue;
            var colorStr = String(r.MapLineColor || '#000000').replace('#', '');
            if (!/^[0-9a-fA-F]{6}$/.test(colorStr)) colorStr = '000000';
            colorStr = colorStr.toUpperCase();
            var rid = (typeof r.RouteID === 'number') ? r.RouteID : parseInt(r.RouteID, 10);
            if (!rid || isNaN(rid)) rid = 0;
            filtered.push({ name: rname, color: colorStr, routeId: rid });
          }
        }

        var palette = [];
        var paletteIndex = {};
        for (var i = 0; i < filtered.length; i++) {
          var c = filtered[i].color;
          if (paletteIndex[c] === undefined && palette.length < 32) {
            paletteIndex[c] = palette.length;
            palette.push(c);
          }
        }

        var lines = [];
        lines.push('P;' + palette.join(';'));
        for (var i = 0; i < filtered.length; i++) {
          var idx = paletteIndex[filtered[i].color];
          if (idx === undefined) idx = 0;
          lines.push(filtered[i].name + ';' + idx + ';' + filtered[i].routeId);
        }
        var payload = lines.join('\n');

        console.log('Sending ' + filtered.length + ' routes, ' +
                    palette.length + ' colors (' + payload.length + ' bytes raw)');

        sendChunked(payload, function() {
          console.log('All route chunks sent');
        });
      } catch (err) {
        console.log('Routes parse error: ' + err.message);
        sendChunked('P;\n', null);
      }
    } else {
      console.log('Routes HTTP error: ' + rxhr.status);
      sendChunked('P;\n', null);
    }
  };

  rxhr.onerror = function() {
    console.log('Routes network error');
    sendChunked('P;\n', null);
  };
  rxhr.ontimeout = function() {
    console.log('Routes timeout');
    sendChunked('P;\n', null);
  };
  rxhr.send();
}

// Quantize lat/lon to ~1m for shared endpoint matching.
function pointKey(lat, lon) {
  return Math.round(lat * 1e5) + ':' + Math.round(lon * 1e5);
}

// Chain stops by matching last point of one to first of next.
function orderStops(stops) {
  var n = stops.length;
  if (n <= 1) return stops.slice();

  var firstKey = new Array(n);
  var lastKey  = new Array(n);
  for (var i = 0; i < n; i++) {
    var mp = stops[i].MapPoints || [];
    if (mp.length === 0) {
      firstKey[i] = null;
      lastKey[i]  = null;
    } else {
      firstKey[i] = pointKey(mp[0].Latitude, mp[0].Longitude);
      lastKey[i]  = pointKey(mp[mp.length - 1].Latitude,
                             mp[mp.length - 1].Longitude);
    }
  }

  var lastToIdx = {};
  for (var i = 0; i < n; i++) {
    if (lastKey[i] !== null) lastToIdx[lastKey[i]] = i;
  }

  var start = -1;
  for (var i = 0; i < n; i++) {
    if (firstKey[i] === null) continue;
    if (lastToIdx[firstKey[i]] === undefined) { start = i; break; }
  }
  if (start < 0) {
    for (var i = 0; i < n; i++) {
      if (firstKey[i] !== null) { start = i; break; }
    }
    if (start < 0) start = 0;
  }

  var visited = new Array(n);
  var ordered = [];
  var cur = start;
  while (cur >= 0 && cur < n && !visited[cur]) {
    visited[cur] = true;
    ordered.push(stops[cur]);

    var next = -1;
    if (lastKey[cur] !== null) {
      for (var j = 0; j < n; j++) {
        if (!visited[j] && firstKey[j] === lastKey[cur]) {
          next = j;
          break;
        }
      }
    }
    cur = next;
  }

  for (var i = 0; i < n; i++) {
    if (!visited[i]) ordered.push(stops[i]);
  }

  console.log('orderStops: ' + n + ' -> ordered ' + ordered.length);
  return ordered;
}

// Stops payload: "<name>;<lat>;<lon>;<stop_id>\n..."
function handleFetchStops(routeId) {
  if (allAgencies.length === 0) {
    console.log('No agencies, cannot fetch stops');
    sendStopChunks('', null);
    return;
  }
  if (!currentApiKey) {
    console.log('No API key yet — fetching config first');
    handleFetchConfig();
    sendStopChunks('', null);
    return;
  }
  if (routeId === undefined || routeId === null || routeId === 0) {
    console.log('handleFetchStops: invalid routeId=' + routeId);
    sendStopChunks('', null);
    return;
  }

  var stopsUrl = buildAgencyUrl('/Services/JSONPRelay.svc/GetStops');
  console.log('Fetching stops for route ' + routeId + ' from: ' + stopsUrl);

  var xhr = new XMLHttpRequest();
  xhr.open('GET', stopsUrl);
  xhr.timeout = 15000;

  xhr.onload = function() {
    if (xhr.status !== 200) {
      console.log('Stops HTTP error: ' + xhr.status);
      sendStopChunks('', null);
      return;
    }
    try {
      var all = JSON.parse(xhr.responseText);
      if (!Array.isArray(all)) {
        sendStopChunks('', null);
        return;
      }

      var filtered = [];
      for (var i = 0; i < all.length; i++) {
        if (Number(all[i].RouteID) === Number(routeId)) filtered.push(all[i]);
      }
      console.log('GetStops: ' + all.length + ' total, ' +
                  filtered.length + ' for route ' + routeId);

      var ordered = orderStops(filtered);

      var lines = [];
      for (var i = 0; i < ordered.length; i++) {
        var s = ordered[i];
        var nm = (s.Description || '').replace(/[\r\n;]/g, ' ');
        var lat = (typeof s.Latitude  === 'number') ? s.Latitude  : parseFloat(s.Latitude);
        var lon = (typeof s.Longitude === 'number') ? s.Longitude : parseFloat(s.Longitude);
        if (!isFinite(lat) || !isFinite(lon)) continue;
        var sid = s.StopID;
        if (sid === undefined) sid = s.StopId;
        if (sid === undefined) sid = s.RouteStopID;
        if (sid === undefined) sid = s.RouteStopId;
        if (sid === undefined) sid = 0;
        lines.push(nm + ';' + lat.toFixed(7) + ';' + lon.toFixed(7) +
                   ';' + sid);
      }

      var payload = lines.join('\n');
      console.log('Sending ' + lines.length + ' stops for route ' +
                  routeId + ' (' + payload.length + ' bytes raw)');
      sendStopChunks(payload, function() {
        console.log('All stop chunks sent for route ' + routeId);
      });
    } catch (err) {
      console.log('Stops parse error: ' + err.message);
      sendStopChunks('', null);
    }
  };

  xhr.onerror = function() {
    console.log('Stops network error');
    sendStopChunks('', null);
  };
  xhr.ontimeout = function() {
    console.log('Stops timeout');
    sendStopChunks('', null);
  };
  xhr.send();
}

// Arrivals payload: "<route name>|<seconds>|<capacity pct>\n..."
// capacity pct is -1 when unavailable.
function handleFetchStopArrivals(routeId, stopId) {
  if (allAgencies.length === 0 || !currentApiKey ||
      !routeId || !stopId) {
    console.log('handleFetchStopArrivals: missing prerequisites ' +
                '(route=' + routeId + ' stop=' + stopId + ')');
    sendArrivalChunks('', null);
    return;
  }

  var arrivalsUrl = buildAgencyUrl('/Services/JSONPRelay.svc/GetStopArrivalTimes') +
                    '&routeIds=' + routeId + '&version=2';
  var capUrl = 'https://' + allAgencies[currentIndex].domain +
               '/Services/JSONPRelay.svc/GetVehicleCapacities' +
               '?apiKey=' + encodeURIComponent(currentApiKey) +
               '&isPublicMap=true';

  console.log('Fetching arrivals: ' + arrivalsUrl);

  var axhr = new XMLHttpRequest();
  axhr.open('GET', arrivalsUrl);
  axhr.timeout = 15000;

  axhr.onload = function() {
    if (axhr.status !== 200) {
      console.log('Arrivals HTTP ' + axhr.status);
      sendArrivalChunks('', null);
      return;
    }
    var arrivals;
    try { arrivals = JSON.parse(axhr.responseText); }
    catch (e) {
      console.log('Arrivals parse error: ' + e.message);
      sendArrivalChunks('', null);
      return;
    }
    if (!Array.isArray(arrivals)) { sendArrivalChunks('', null); return; }

    var cxhr = new XMLHttpRequest();
    cxhr.open('GET', capUrl);
    cxhr.timeout = 15000;
    cxhr.onload = function() {
      var caps = {};
      if (cxhr.status === 200) {
        try {
          var cd = JSON.parse(cxhr.responseText);
          if (Array.isArray(cd)) {
            for (var i = 0; i < cd.length; i++) {
              caps[cd[i].VehicleID] = Math.round((cd[i].Percentage || 0) * 100);
            }
          }
        } catch (e) {
          console.log('Capacities parse error: ' + e.message);
        }
      }
      buildArrivalPayload(arrivals, stopId, caps);
    };
    cxhr.onerror = cxhr.ontimeout = function() {
      console.log('Capacities fetch failed; proceeding without');
      buildArrivalPayload(arrivals, stopId, {});
    };
    cxhr.send();
  };
  axhr.onerror = axhr.ontimeout = function() {
    console.log('Arrivals network error/timeout');
    sendArrivalChunks('', null);
  };
  axhr.send();
}

function buildArrivalPayload(arrivals, stopId, caps) {
  var entry = null;
  for (var i = 0; i < arrivals.length; i++) {
    var a = arrivals[i];
    if (Number(a.StopId) === Number(stopId) ||
        Number(a.RouteStopId) === Number(stopId) ||
        Number(a.StopID) === Number(stopId) ||
        Number(a.RouteStopID) === Number(stopId)) {
      entry = a;
      break;
    }
  }
  if (!entry) {
    console.log('Stop ' + stopId + ' not present in arrivals response');
    sendArrivalChunks('', null);
    return;
  }

  var times = (entry.Times || []).slice();
  times.sort(function(x, y) { return (x.Seconds || 0) - (y.Seconds || 0); });

  var routeName = String(entry.RouteDescription || '').replace(/[\r\n|]/g, ' ');
  var lines = [];
  for (var i = 0; i < times.length; i++) {
    var t = times[i];
    if (t.IsDeparted) continue;
    var sec = t.Seconds || 0;
    var vid = t.VehicleId;
    var pct = (vid !== undefined && caps[vid] !== undefined) ? caps[vid] : -1;
    var busName = (vid !== undefined && vid !== null)
                  ? ('Bus ' + vid)
                  : routeName;
    lines.push(busName + '|' + sec + '|' + pct);
  }
  var payload = lines.join('\n');
  console.log('Sending ' + lines.length + ' arrivals for stop ' + stopId +
              ' (' + payload.length + ' bytes)');
  sendArrivalChunks(payload, null);
}

// Commands: 0=INIT, 1=UP, 2=DOWN, 3=JUMP_TO_LETTER, 4=JUMP_TO_DOMAIN.
function handleCommand(command, letter, jumpDomain) {
  if (!allAgencies || allAgencies.length === 0) {
    sendError('NO_AGENCIES');
    return;
  }

  currentApiKey = '';

  var n = allAgencies.length;
  if (command === 0) {
    currentIndex = 0;
  } else if (command === 1) {
    currentIndex = (currentIndex - 1 + n) % n;
  } else if (command === 2) {
    currentIndex = (currentIndex + 1) % n;
  } else if (command === 3 && letter) {
    var target = String.fromCharCode(letter).toUpperCase();
    var found = false;
    for (var i = 0; i < n; i++) {
      var firstChar = (allAgencies[i].name || '').charAt(0).toUpperCase();
      if (firstChar === target) { currentIndex = i; found = true; break; }
    }
    if (!found) {
      for (var i = 0; i < n; i++) {
        var firstChar = (allAgencies[i].name || '').charAt(0).toUpperCase();
        if (firstChar > target) { currentIndex = i; found = true; break; }
      }
      if (!found) currentIndex = 0;
    }
    console.log('Jump to ' + target + ' -> index ' + currentIndex);
  } else if (command === 4 && jumpDomain) {
    var found = false;
    for (var i = 0; i < n; i++) {
      if (allAgencies[i].domain === jumpDomain) {
        currentIndex = i;
        found = true;
        break;
      }
    }
    if (!found) currentIndex = 0;
    console.log('Jump to domain ' + jumpDomain + ' -> index ' + currentIndex);
  }

  var prevIndex = (currentIndex - 1 + n) % n;
  var nextIndex = (currentIndex + 1) % n;

  var current = allAgencies[currentIndex];
  var prev = allAgencies[prevIndex];
  var next = allAgencies[nextIndex];

  var payload = {
    'PREV_NAME': prev.name,
    'CURRENT_NAME': current.name,
    'CURRENT_URL': current.domain,
    'NEXT_NAME': next.name
  };

  console.log('Sending for index ' + currentIndex + ': prev=' + prev.name +
              ', curr=' + current.name + ', next=' + next.name);

  Pebble.sendAppMessage(
    payload,
    function() { console.log('Data sent successfully'); },
    function(err) { console.log('Failed to send: ' + JSON.stringify(err)); }
  );
}

// Fallback parser for GetClients; emulator V8 chokes on JSON.parse.
function extractClientsFromText(text) {
  var clients = [];
  var i = 0;
  var len = text.length;

  while (i < len && text.charAt(i) !== '[') i++;
  if (i >= len) return clients;
  i++;

  while (i < len) {
    while (i < len && (text.charAt(i) === ',' ||
                       text.charAt(i) === ' ' ||
                       text.charAt(i) === '\n' ||
                       text.charAt(i) === '\r' ||
                       text.charAt(i) === '\t')) i++;
    if (i >= len || text.charAt(i) === ']') break;
    if (text.charAt(i) !== '{') break;

    var start = i;
    var depth = 0;
    var inString = false;
    var escaped = false;
    var end = -1;
    while (i < len) {
      var c = text.charAt(i);
      if (escaped) {
        escaped = false;
      } else if (c === '\\' && inString) {
        escaped = true;
      } else if (c === '"') {
        inString = !inString;
      } else if (!inString) {
        if (c === '{') depth++;
        else if (c === '}') {
          depth--;
          if (depth === 0) { end = i; break; }
        }
      }
      i++;
    }
    if (end < 0) break;

    var objText = text.substring(start, end + 1);
    var name = extractJsonString(objText, 'Name');
    var webAddress = extractJsonString(objText, 'WebAddress');
    var isProtected = extractJsonBool(objText, 'IsPasswordProtected');

    if (name !== null && webAddress !== null) {
      clients.push({
        Name: name,
        WebAddress: webAddress,
        IsPasswordProtected: isProtected === true
      });
    }

    i = end + 1;
  }

  console.log('Fallback parser extracted ' + clients.length + ' client objects');
  return clients;
}

function extractJsonString(text, key) {
  var needle = '"' + key + '":';
  var idx = text.indexOf(needle);
  if (idx < 0) return null;
  idx += needle.length;
  while (idx < text.length &&
         (text.charAt(idx) === ' ' || text.charAt(idx) === '\t')) idx++;
  if (text.charAt(idx) !== '"') return null;
  idx++;
  var start = idx;
  var escaped = false;
  while (idx < text.length) {
    var c = text.charAt(idx);
    if (escaped) {
      escaped = false;
    } else if (c === '\\') {
      escaped = true;
    } else if (c === '"') {
      break;
    }
    idx++;
  }
  if (idx >= text.length) return null;
  var raw = text.substring(start, idx);
  return unescapeJsonString(raw);
}

function extractJsonBool(text, key) {
  var needle = '"' + key + '":';
  var idx = text.indexOf(needle);
  if (idx < 0) return null;
  idx += needle.length;
  while (idx < text.length &&
         (text.charAt(idx) === ' ' || text.charAt(idx) === '\t')) idx++;
  if (text.substr(idx, 4) === 'true') return true;
  if (text.substr(idx, 5) === 'false') return false;
  return null;
}

function unescapeJsonString(s) {
  if (s.indexOf('\\') < 0) return s;
  return s.replace(/\\u([0-9a-fA-F]{4})/g, function(m, hex) {
    return String.fromCharCode(parseInt(hex, 16));
  }).replace(/\\"/g, '"')
    .replace(/\\n/g, '\n')
    .replace(/\\r/g, '\r')
    .replace(/\\t/g, '\t')
    .replace(/\\\//g, '/')
    .replace(/\\\\/g, '\\');
}

// Hand-rolled comparator; emulator ICU lacks localeCompare.
function compareAgencyNames(a, b) {
  var an = a.name.toLowerCase();
  var bn = b.name.toLowerCase();
  var len = an.length < bn.length ? an.length : bn.length;
  for (var i = 0; i < len; i++) {
    var ca = an.charCodeAt(i);
    var cb = bn.charCodeAt(i);
    if (ca < cb) return -1;
    if (ca > cb) return 1;
  }
  if (an.length < bn.length) return -1;
  if (an.length > bn.length) return 1;
  return 0;
}

function fetchAgencyList() {
  if (agenciesLoading) return;
  agenciesLoading = true;

  console.log('Fetching from admin.ridesystems.net API...');

  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'https://admin.ridesystems.net/api/Clients/GetClients');
  xhr.timeout = 15000;

  xhr.onload = function() {
    if (xhr.status !== 200) {
      console.log('HTTP status: ' + xhr.status);
      agenciesLoading = false;
      sendError('HTTP_' + xhr.status);
      return;
    }

    var text = xhr.responseText;
    console.log('Response length: ' + text.length);

    if (text.charCodeAt(0) === 0xFEFF) {
      text = text.substring(1);
    }

    var data = null;
    try {
      data = JSON.parse(text);
    } catch (err) {
      console.log('JSON.parse failed (' + err.message + '); using fallback parser');
      data = extractClientsFromText(text);
    }

    if (!Array.isArray(data)) {
      agenciesLoading = false;
      sendError('INVALID_DATA');
      return;
    }

    allAgencies = parseAgenciesFromJSON(data);
    agenciesLoaded = true;
    agenciesLoading = false;
    console.log('Parsed ' + allAgencies.length + ' agencies');

    while (pendingMessages.length > 0) {
      processAppMessage(pendingMessages.shift());
    }
  };

  xhr.onerror = function() { agenciesLoading = false; sendError('NETWORK'); };
  xhr.ontimeout = function() { agenciesLoading = false; sendError('TIMEOUT'); };
  xhr.send();
}

function parseAgenciesFromJSON(data) {
  var results = [];
  for (var i = 0; i < data.length; i++) {
    var item = data[i];
    if (item.IsPasswordProtected === true) continue;
    var name = item.Name || '';
    var webAddress = item.WebAddress || '';
    if (!name || name.length === 0) continue;
    if (!webAddress || webAddress.length === 0) continue;
    var domain = webAddress.replace(/^https?:\/\//i, '').replace(/\/.*$/, '');
    if (domain && domain.length > 0) {
      results.push({ name: name, domain: domain });
    }
  }
  var unique = [];
  var seen = {};
  for (var i = 0; i < results.length; i++) {
    if (!seen[results[i].domain]) {
      seen[results[i].domain] = true;
      unique.push(results[i]);
    }
  }
  unique.sort(compareAgencyNames);
  return unique;
}

function sendError(code) {
  var message = 'ERROR_' + code;
  console.log('Sending error: ' + message);
  Pebble.sendAppMessage(
    { 'ERROR': message },
    function() {},
    function(err) { console.log('Failed to send error: ' + JSON.stringify(err)); }
  );
}

setTimeout(function() {
  if (!agenciesLoaded && !agenciesLoading) {
    console.log('App loaded, fetching agency list...');
    fetchAgencyList();
  }
}, 1000);

console.log('TransLoc JS ready');