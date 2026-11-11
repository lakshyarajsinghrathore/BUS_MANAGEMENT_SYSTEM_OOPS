/* ── passenger.js ───────────────────────────────────────
   Handles: route list, route selection, stop display,
            live bus location polling every 5 s
   ──────────────────────────────────────────────────── */

(function () {
  /* ── Auth guard ─── */
  const token = localStorage.getItem('bus_token');
  const role  = localStorage.getItem('bus_role');
  const name  = localStorage.getItem('bus_name');

  if (!token || role !== 'passenger') {
    window.location.href = '../index.html';
    return;
  }

  /* ── Populate user info ─── */
  const safeName = (name || '').trim() || 'Passenger';
  document.getElementById('user-name').textContent   = safeName;
  document.getElementById('user-avatar').textContent = safeName[0].toUpperCase();

  function escapeHtml(str) {
    return String(str ?? '').replace(/[&<>"']/g, ch => ({
      '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    }[ch]));
  }

  function parseServerTimestamp(ts) {
    if (!ts) return new Date();
    if (typeof ts === 'string' && /^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(ts)) {
      return new Date(ts.replace(' ', 'T') + 'Z');
    }
    const parsed = new Date(ts);
    return isNaN(parsed.getTime()) ? new Date() : parsed;
  }

  /* ── Mobile view switcher ─── */
  const mobileTabBtns = document.querySelectorAll('.mobile-tab-btn[data-mobile-tab]');
  const mobileMapBar = document.getElementById('mobile-map-bar');
  const mobileMapBus = document.getElementById('mobile-map-bus');
  const mobileMapRoute = document.getElementById('mobile-map-route');
  const mobileMapSeats = document.getElementById('mobile-map-seats');
  const mobileBtnStops = document.getElementById('mobile-btn-stops');
  const mobileLogoutBtn = document.getElementById('mobile-btn-logout');

  function setMobileView(view) {
    document.body.dataset.mobileView = view;
    mobileTabBtns.forEach(btn => {
      const isActive = btn.dataset.mobileTab === view;
      btn.classList.toggle('active', isActive);
      btn.setAttribute('aria-selected', isActive ? 'true' : 'false');
    });
    if (view === 'map') {
      setTimeout(() => map.invalidateSize(), 60);
    }
  }

  if (mobileTabBtns.length) {
    setMobileView('panel');
    mobileTabBtns.forEach(btn => {
      btn.addEventListener('click', () => setMobileView(btn.dataset.mobileTab));
    });
  }
  window.addEventListener('resize', () => {
    setTimeout(() => map.invalidateSize(), 80);
  });

  /* ── Map setup ─── */
  const map = L.map('map', { zoomControl: true }).setView([24.5854, 73.7125], 12);

  L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
    maxZoom: 19
  }).addTo(map);

  /* ── State ─── */
  let selectedRoute   = null;   // { id, driver_id, bus_number, route_name, stops[] }
  let stopMarkers     = [];
  let routePolyline   = null;
  let busMarker       = null;
  let pollTimer       = null;

  /* ── Smooth motion & bearing state ─── */
  let currentLat      = null;
  let currentLng      = null;
  let currentBearing  = 0;
  let animFrameId     = null;

  /* ── Student / Nearest Stop State ─── */
  let studentMarker   = null;
  let walkingPolyline = null;
  let studentLat      = null;
  let studentLng      = null;

  /* ── Co-Location & Seating Occupancy State ─── */
  let passengerWatchId   = null;
  let passengerGpsLat    = null;
  let passengerGpsLng    = null;
  let coLocationStart    = null;
  let isOnBoard          = false;
  let heartbeatInterval  = null;

  /* ── DOM refs ─── */
  const sectionRoutes = document.getElementById('section-routes');
  const sectionDetail = document.getElementById('section-detail');
  const routeList     = document.getElementById('route-list');
  const emptyRoutes   = document.getElementById('empty-routes');
  const mapPlaceholder= document.getElementById('map-placeholder');
  const mapLocateBtn  = document.getElementById('map-locate-btn');

  /* ── Bearing & Navigation Math ─── */
  function calculateBearing(lat1, lng1, lat2, lng2) {
    const toRad = Math.PI / 180;
    const toDeg = 180 / Math.PI;
    const phi1 = lat1 * toRad;
    const phi2 = lat2 * toRad;
    const dLambda = (lng2 - lng1) * toRad;
    const y = Math.sin(dLambda) * Math.cos(phi2);
    const x = Math.cos(phi1) * Math.sin(phi2) - Math.sin(phi1) * Math.cos(phi2) * Math.cos(dLambda);
    const brng = Math.atan2(y, x) * toDeg;
    return (brng + 360) % 360;
  }

  function shortestAngleDiff(target, current) {
    return ((((target - current) % 360) + 540) % 360) - 180;
  }

  function compassDirection(bearing) {
    const directions = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
    const idx = Math.round(bearing / 45) % 8;
    return directions[idx];
  }

  function approxDistance(lat1, lng1, lat2, lng2) {
    const dLat = (lat2 - lat1) * 111320;
    const dLng = (lng2 - lng1) * (111320 * Math.cos(lat1 * (Math.PI / 180)));
    return Math.hypot(dLat, dLng);
  }

  function haversineDistance(lat1, lon1, lat2, lon2) {
    const R = 6371000;
    const dLat = (lat2 - lat1) * Math.PI / 180;
    const dLon = (lon2 - lon1) * Math.PI / 180;
    const a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
              Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
              Math.sin(dLon / 2) * Math.sin(dLon / 2);
    const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
    return R * c;
  }

  function formatDistance(meters) {
    if (meters < 1000) return `${Math.round(meters)} m`;
    return `${(meters / 1000).toFixed(1)} km`;
  }

  function formatWalkTime(meters) {
    const mins = Math.max(1, Math.round(meters / 80));
    return `~${mins} min walk`;
  }

  /* ── Render Seating Occupancy Badge with Number & Percentage ─── */
  function renderOccupancyBadge(capacity, available, availablePct) {
    capacity = capacity || 40;
    available = (available !== undefined && available !== null) ? available : capacity;
    availablePct = (availablePct !== undefined && availablePct !== null) ? availablePct : 100.0;
    
    let statusClass = 'status-green';
    if (availablePct <= 15) {
      statusClass = 'status-red';
    } else if (availablePct <= 50) {
      statusClass = 'status-amber';
    }

    return `
      <div class="occupancy-badge">
        <div class="occupancy-header">
          <span class="occupancy-seats">
            <svg viewBox="0 0 20 20" fill="currentColor" class="occupancy-icon">
              <path d="M7 2a2 2 0 00-2 2v7h10V4a2 2 0 00-2-2H7zM4 13a2 2 0 00-2 2v2a1 1 0 001 1h14a1 1 0 001-1v-2a2 2 0 00-2-2H4z"/>
            </svg>
            <span class="occupancy-count"><strong>${available}</strong> / ${capacity} Seats</span>
          </span>
          <span class="occupancy-percent ${statusClass}-badge">${Math.round(availablePct)}% Free</span>
        </div>
        <div class="occupancy-bar">
          <div class="occupancy-fill ${statusClass}-bg" style="width: ${Math.max(4, Math.min(100, availablePct))}%;"></div>
        </div>
      </div>`;
  }

  function updateDetailOccupancy(capacity, available, availablePct) {
    capacity = capacity || 40;
    available = (available !== undefined && available !== null) ? available : capacity;
    availablePct = (availablePct !== undefined && availablePct !== null) ? availablePct : 100.0;

    let statusClass = 'status-green';
    if (availablePct <= 15) {
      statusClass = 'status-red';
    } else if (availablePct <= 50) {
      statusClass = 'status-amber';
    }

    const textEl = document.getElementById('detail-occupancy-text');
    const badgePctEl = document.getElementById('detail-occupancy-badge-pct');
    const fillEl = document.getElementById('detail-occupancy-fill');

    if (textEl) {
      textEl.innerHTML = `
        <svg viewBox="0 0 20 20" fill="currentColor" class="occupancy-icon">
          <path d="M7 2a2 2 0 00-2 2v7h10V4a2 2 0 00-2-2H7zM4 13a2 2 0 00-2 2v2a1 1 0 001 1h14a1 1 0 001-1v-2a2 2 0 00-2-2H4z"/>
        </svg>
        <span class="occupancy-count"><strong>${available}</strong> / ${capacity} Seats</span>`;
    }
    if (badgePctEl) {
      badgePctEl.textContent = `${Math.round(availablePct)}% Free`;
      badgePctEl.className = `occupancy-percent ${statusClass}-badge`;
    }
    if (fillEl) {
      fillEl.style.width = `${Math.max(4, Math.min(100, availablePct))}%`;
      fillEl.className = `occupancy-fill ${statusClass}-bg`;
    }
    if (mobileMapSeats) {
      mobileMapSeats.textContent = `${available}/${capacity} seats free`;
      mobileMapSeats.className = `mobile-map-seats ${statusClass}-badge`;
    }
  }

  /* ── Co-Location & Automated Ride Trigger ─── */
  function startCoLocationWatcher() {
    if (!navigator.geolocation || passengerWatchId !== null) return;

    passengerWatchId = navigator.geolocation.watchPosition(
      (pos) => {
        passengerGpsLat = pos.coords.latitude;
        passengerGpsLng = pos.coords.longitude;
        evaluateCoLocation();
      },
      () => {},
      { enableHighAccuracy: true, maximumAge: 5000, timeout: 10000 }
    );
  }

  function stopCoLocationWatcher() {
    if (passengerWatchId !== null) {
      navigator.geolocation.clearWatch(passengerWatchId);
      passengerWatchId = null;
    }
    exitOnBoardState();
  }

  function evaluateCoLocation() {
    if (!selectedRoute || currentLat === null || currentLng === null || passengerGpsLat === null || passengerGpsLng === null) {
      return;
    }

    const dist = haversineDistance(passengerGpsLat, passengerGpsLng, currentLat, currentLng);
    const now = Date.now();

    // 1. Proximity check (<= 25 meters)
    if (dist <= 25) {
      if (!coLocationStart) {
        coLocationStart = now;
      } else if (now - coLocationStart >= 20000 && !isOnBoard) {
        // Condition held continuously for 20s! Confirm passenger is ON BOARD!
        enterOnBoardState();
      }
    } else {
      if (!isOnBoard) {
        coLocationStart = null;
      } else if (dist > 50) {
        // Disembarked / distance opened up > 50m
        exitOnBoardState();
      }
    }
  }

  function enterOnBoardState() {
    if (isOnBoard || !selectedRoute) return;
    isOnBoard = true;

    const toast = document.getElementById('onboard-toast');
    const toastMsg = document.getElementById('onboard-toast-msg');
    if (toast && toastMsg) {
      toastMsg.textContent = `You're on board ${selectedRoute.bus_number || 'Bus'} • Seat Counted`;
      toast.classList.remove('hidden');
    }

    sendHeartbeatPulse();
    if (heartbeatInterval) clearInterval(heartbeatInterval);
    heartbeatInterval = setInterval(sendHeartbeatPulse, 10000);
  }

  async function sendHeartbeatPulse() {
    if (!isOnBoard || !selectedRoute) return;
    try {
      await apiCall('POST', `/route/${selectedRoute.id}/heartbeat`);
      // Immediately refresh occupancy display after heartbeat
      const occData = await apiCall('GET', `/route/${selectedRoute.id}/occupancy`);
      if (occData && !occData.error) {
        updateDetailOccupancy(occData.capacity, occData.available_seats, occData.available_percent);
      }
    } catch {}
  }

  async function exitOnBoardState() {
    if (!isOnBoard && coLocationStart === null) return;
    const wasOnBoard = isOnBoard;
    isOnBoard = false;
    coLocationStart = null;

    if (heartbeatInterval) {
      clearInterval(heartbeatInterval);
      heartbeatInterval = null;
    }

    const toast = document.getElementById('onboard-toast');
    if (toast) toast.classList.add('hidden');

    if (wasOnBoard && selectedRoute) {
      try {
        await apiCall('POST', `/route/${selectedRoute.id}/leave`);
      } catch {}
    }
  }

  /* ── Student / "You are here" Marker Icon ─── */
  function makeStudentIcon() {
    return L.divIcon({
      className: '',
      html: `<div class="student-marker-puck"><div class="student-pulse"></div><div class="student-dot"></div></div>`,
      iconSize: [32, 32],
      iconAnchor: [16, 16]
    });
  }

  /* ── Numbered stop marker ─── */
  function makeStopIcon(n) {
    return L.divIcon({
      className: '',
      html: `<div style="width:26px;height:26px;background:#6366f1;border:2px solid #fff;border-radius:50%;display:flex;align-items:center;justify-content:center;color:#fff;font-size:10px;font-weight:700;box-shadow:0 2px 8px rgba(0,0,0,0.5)">${n}</div>`,
      iconSize: [26, 26],
      iconAnchor: [13, 13]
    });
  }

  /* ── Directional Bus Marker Puck with Heading Arrow & Radar Pulse ─── */
  function makeBusIcon(bearing = 0) {
    return L.divIcon({
      className: 'bus-leaflet-icon',
      html: `
        <div class="bus-puck-container">
          <div class="bus-pulse-ring"></div>
          <div class="bus-rotator" style="transform: rotate(${bearing.toFixed(1)}deg)">
            <div class="bus-arrow-pointer">
              <svg viewBox="0 0 16 12" fill="none">
                <path d="M8 0L16 12L8 8.5L0 12L8 0Z" fill="#6366f1"/>
              </svg>
            </div>
            <div class="bus-puck-core">
              <svg viewBox="0 0 32 32" fill="none">
                <rect x="7" y="3" width="18" height="26" rx="5" fill="#6366f1"/>
                <rect x="9.5" y="6" width="13" height="6" rx="1.5" fill="#ffffff" opacity="0.95"/>
                <rect x="9" y="14" width="3.5" height="5" rx="1" fill="#ffffff" opacity="0.8"/>
                <rect x="19.5" y="14" width="3.5" height="5" rx="1" fill="#ffffff" opacity="0.8"/>
                <rect x="9" y="21" width="3.5" height="5" rx="1" fill="#ffffff" opacity="0.8"/>
                <rect x="19.5" y="21" width="3.5" height="5" rx="1" fill="#ffffff" opacity="0.8"/>
                <circle cx="11" cy="4.5" r="1.2" fill="#fbbf24"/>
                <circle cx="21" cy="4.5" r="1.2" fill="#fbbf24"/>
                <rect x="9.5" y="28" width="3.5" height="1" rx="0.5" fill="#ef4444"/>
                <rect x="19" y="28" width="3.5" height="1" rx="0.5" fill="#ef4444"/>
              </svg>
            </div>
          </div>
        </div>`,
      iconSize: [48, 48],
      iconAnchor: [24, 24]
    });
  }

  /* ── Load all active routes ─── */
  async function loadRoutes() {
    const refreshBtn = document.getElementById('btn-refresh');
    refreshBtn.classList.add('spinning');

    try {
      const data = await apiCall('GET', '/routes/all');
      refreshBtn.classList.remove('spinning');

      routeList.innerHTML = '';
      const routes = Array.isArray(data) ? data : [];

      if (routes.length === 0) {
        const titleEl = document.getElementById('empty-routes-title');
        const descEl  = document.getElementById('empty-routes-desc');
        if (titleEl) titleEl.textContent = 'No buses are active right now';
        if (descEl)  descEl.textContent  = 'Check back soon or refresh';
        routeList.appendChild(emptyRoutes);
        emptyRoutes.classList.remove('hidden');
        return;
      }

      emptyRoutes.classList.add('hidden');
      routes.forEach(r => {
        /* normalise field names — backend returns route_id / driver_id / bus_id */
        const route = {
          id:        r.id        ?? r.route_id,
          driver_id: r.driver_id,
          bus_number:r.bus_number ?? `Bus ${r.bus_id || ''}`,
          route_name:r.route_name,
          stop_count:r.stop_count ?? 0,
          capacity:  r.capacity ?? 40,
          available_seats: r.available_seats,
          available_percent: r.available_percent,
          occupied_seats: r.occupied_seats
        };
        const card = document.createElement('div');
        card.className = 'route-card';
        card.dataset.id = route.id;
        card.setAttribute('role', 'button');
        card.setAttribute('tabindex', '0');
        card.innerHTML = `
          <div class="route-card-top">
            <span class="route-card-bus">${escapeHtml(route.bus_number || 'BUS')}</span>
            <span class="badge badge-live"><span class="badge-dot"></span>Live</span>
          </div>
          <div class="route-card-name">${escapeHtml(route.route_name || 'Route')}</div>
          <div class="route-card-meta">${route.stop_count} stops</div>
          ${renderOccupancyBadge(route.capacity, route.available_seats, route.available_percent)}`;
        card.addEventListener('click', () => selectRoute(route));
        card.addEventListener('keydown', (e) => {
          if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); selectRoute(route); }
        });
        routeList.appendChild(card);
      });
    } catch {
      refreshBtn.classList.remove('spinning');
      routeList.innerHTML = '';
      const titleEl = document.getElementById('empty-routes-title');
      const descEl  = document.getElementById('empty-routes-desc');
      if (titleEl) titleEl.textContent = 'Could not load routes';
      if (descEl)  descEl.textContent  = 'Make sure the backend server is running and tap refresh';
      emptyRoutes.classList.remove('hidden');
      routeList.appendChild(emptyRoutes);
    }
  }

  /* ── Select a route ─── */
  async function selectRoute(routeSummary) {
    /* use pre-supplied stops (demo mode) or fetch from API */
    let stops = routeSummary.stops || [];
    if (!stops.length) {
      try {
        const data = await apiCall('GET', `/route/${routeSummary.id}/stops`);
        stops = Array.isArray(data) ? data : (data.stops || []);
      } catch {}
    }

    selectedRoute = { ...routeSummary, stops };

    /* switch sidebar view */
    sectionRoutes.classList.add('hidden');
    sectionDetail.classList.remove('hidden');

    document.getElementById('detail-bus').textContent        = routeSummary.bus_number || 'BUS';
    document.getElementById('detail-name').textContent       = routeSummary.route_name || 'Route';
    document.getElementById('detail-stops-count').textContent= `${stops.length} stops`;

    /* render stop list */
    const stopListEl = document.getElementById('detail-stop-list');
    stopListEl.innerHTML = '';
    stops.forEach((s, i) => {
      const div = document.createElement('div');
      div.className = 'stop-item';
      div.innerHTML = `
        <div class="stop-num">${i + 1}</div>
        <div>
          <div class="stop-name">${escapeHtml(s.name || `Stop ${i+1}`)}</div>
          <div class="stop-coords">${parseFloat(s.lat).toFixed(5)}, ${parseFloat(s.lng).toFixed(5)}</div>
        </div>`;
      stopListEl.appendChild(div);
    });

    if (mobileMapBus) mobileMapBus.textContent = routeSummary.bus_number || 'BUS';
    if (mobileMapRoute) mobileMapRoute.textContent = routeSummary.route_name || 'Route';
    if (mobileMapBar) mobileMapBar.classList.remove('hidden');

    /* hide placeholder */
    mapPlaceholder.classList.add('hidden');
    clearNearestStop();

    if (window.innerWidth <= 768) {
      setMobileView('map');
    }

    /* draw route on map (defer slightly if container just became visible on mobile) */
    setTimeout(() => {
      map.invalidateSize();
      drawRoute(stops);
      if (mapLocateBtn) mapLocateBtn.classList.remove('hidden');
    }, window.innerWidth <= 768 ? 80 : 0);

    /* update occupancy block in route detail view */
    updateDetailOccupancy(routeSummary.capacity, routeSummary.available_seats, routeSummary.available_percent);

    /* start polling bus location and watching co-location */
    startPolling(routeSummary.driver_id);
    startCoLocationWatcher();
  }

  /* ── Segment-by-segment road routing (avoids OSRM detour artifacts) ─── */
  async function fetchRoadPath(stops) {
    const allLatLng = [];
    for (let i = 0; i < stops.length - 1; i++) {
      const a = stops[i], b = stops[i + 1];
      try {
        const res  = await fetch(`https://router.project-osrm.org/route/v1/driving/${a.lng},${a.lat};${b.lng},${b.lat}?overview=full&geometries=geojson`);
        const data = await res.json();
        if (data.code === 'Ok' && data.routes[0]) {
          const seg = data.routes[0].geometry.coordinates.map(c => [c[1], c[0]]);
          if (allLatLng.length) seg.shift();
          allLatLng.push(...seg);
          continue;
        }
      } catch {}
      if (!allLatLng.length) allLatLng.push([a.lat, a.lng]);
      allLatLng.push([b.lat, b.lng]);
    }
    return allLatLng;
  }

  /* ── Draw stops + road-following route on map ─── */
  async function drawRoute(stops) {
    clearMap();
    if (!stops.length) return;

    stops.forEach((s, i) => {
      const m = L.marker([s.lat, s.lng], { icon: makeStopIcon(i + 1) })
        .addTo(map)
        .bindPopup(`<b>${escapeHtml(s.name || `Stop ${i+1}`)}</b>`);
      stopMarkers.push(m);
    });

    if (stops.length >= 2) {
      const latLngs = await fetchRoadPath(stops);
      routePolyline = L.polyline(
        latLngs.length >= 2 ? latLngs : stops.map(s => [s.lat, s.lng]),
        { color: '#6366f1', weight: 3, opacity: 0.85, dashArray: '8 5' }
      ).addTo(map);
    }

    const bounds = L.latLngBounds(stops.map(s => [s.lat, s.lng]));
    map.fitBounds(bounds, { padding: [40, 40] });
  }

  /* ── Clear nearest stop markers and UI ─── */
  function clearNearestStop() {
    if (studentMarker)   { map.removeLayer(studentMarker);   studentMarker   = null; }
    if (walkingPolyline) { map.removeLayer(walkingPolyline); walkingPolyline = null; }
    studentLat = null;
    studentLng = null;

    const banner = document.getElementById('nearest-stop-banner');
    if (banner) banner.classList.add('hidden');

    document.querySelectorAll('.stop-item.is-nearest').forEach(el => {
      el.classList.remove('is-nearest');
      const b = el.querySelector('.badge-nearest');
      if (b) b.remove();
    });
  }

  /* ── Clear map layers ─── */
  function clearMap() {
    if (animFrameId) {
      cancelAnimationFrame(animFrameId);
      animFrameId = null;
    }
    currentLat     = null;
    currentLng     = null;
    currentBearing = 0;

    clearNearestStop();

    stopMarkers.forEach(m => map.removeLayer(m));
    stopMarkers = [];
    if (routePolyline) { map.removeLayer(routePolyline); routePolyline = null; }
    if (busMarker)     { map.removeLayer(busMarker);     busMarker     = null; }
    if (mapLocateBtn)  mapLocateBtn.classList.add('hidden');
  }

  /* ── Smooth Marker Interpolation & Bearing via requestAnimationFrame ─── */
  function updateBusPosition(targetLat, targetLng) {
    // 1. Initial GPS coordinate
    if (currentLat === null || currentLng === null) {
      currentLat = targetLat;
      currentLng = targetLng;
      if (!busMarker) {
        busMarker = L.marker([currentLat, currentLng], {
          icon: makeBusIcon(currentBearing),
          zIndexOffset: 1000
        }).addTo(map).bindPopup('<b>Live Bus</b>');
      } else {
        busMarker.setLatLng([currentLat, currentLng]);
      }
      return;
    }

    // 2. Filter micro-jitter (< 1.5m movement)
    const dist = approxDistance(currentLat, currentLng, targetLat, targetLng);
    if (dist < 1.5) return;

    // 3. Compute forward bearing & shortest angular difference
    const targetBearing = calculateBearing(currentLat, currentLng, targetLat, targetLng);
    const startBearing  = currentBearing;
    const bearingDelta  = shortestAngleDiff(targetBearing, startBearing);

    const startLat  = currentLat;
    const startLng  = currentLng;
    const duration  = 4600; // Interpolate across 4.6s of the 5.0s polling cycle
    const startTime = performance.now();

    if (animFrameId) cancelAnimationFrame(animFrameId);

    function step(now) {
      const elapsed = now - startTime;
      const progress = Math.min(elapsed / duration, 1);

      // Interpolate position and bearing
      currentLat     = startLat + (targetLat - startLat) * progress;
      currentLng     = startLng + (targetLng - startLng) * progress;
      currentBearing = (startBearing + bearingDelta * progress + 360) % 360;

      if (busMarker) {
        busMarker.setLatLng([currentLat, currentLng]);

        const iconEl = busMarker.getElement();
        if (iconEl) {
          const rotator = iconEl.querySelector('.bus-rotator');
          if (rotator) {
            rotator.style.transform = `rotate(${currentBearing.toFixed(1)}deg)`;
          }
        }
      }

      if (progress < 1) {
        animFrameId = requestAnimationFrame(step);
      } else {
        animFrameId = null;
      }
    }

    animFrameId = requestAnimationFrame(step);
  }

  /* ── Poll bus GPS location every 5 s ─── */
  function startPolling(driverId) {
    if (pollTimer) clearInterval(pollTimer);
    fetchBusLocation(driverId);
    pollTimer = setInterval(() => fetchBusLocation(driverId), 5000);
  }

  async function fetchBusLocation(driverId) {
    try {
      const data = await apiCall('GET', `/location/${driverId}`);
      if (!data || data.error) return;
      const lat = parseFloat(data.lat);
      const lng = parseFloat(data.lng);
      if (isNaN(lat) || isNaN(lng)) return;

      const timeStr = parseServerTimestamp(data.updated_at).toLocaleTimeString();
      const compass = compassDirection(currentBearing);
      document.getElementById('bus-loc-coords').textContent =
        `${lat.toFixed(6)}, ${lng.toFixed(6)}`;
      document.getElementById('bus-loc-time').textContent =
        `Updated ${timeStr} • ${Math.round(currentBearing)}° ${compass}`;

      updateBusPosition(lat, lng);
      evaluateCoLocation();

      if (data.occupancy) {
        updateDetailOccupancy(data.occupancy.capacity, data.occupancy.available_seats, data.occupancy.available_percent);
      }
    } catch { /* silent */ }
  }

  /* ── Find My Nearest Stop (Student Geolocation) ─── */
  async function locateNearestStop() {
    if (!selectedRoute || !selectedRoute.stops || !selectedRoute.stops.length) {
      alert('Please select an active route with stops first.');
      return;
    }

    if (!navigator.geolocation) {
      alert('Geolocation is not supported by your browser.');
      return;
    }

    const btn = document.getElementById('btn-locate-me');
    const mapBtn = document.getElementById('map-locate-btn');
    const btnText = document.getElementById('locate-btn-text');

    if (btn) btn.classList.add('locating');
    if (mapBtn) mapBtn.classList.add('locating');
    if (btnText) btnText.textContent = 'Locating…';

    navigator.geolocation.getCurrentPosition(
      async (pos) => {
        if (btn) btn.classList.remove('locating');
        if (mapBtn) mapBtn.classList.remove('locating');
        if (btnText) btnText.textContent = 'Locate Me';

        const uLat = pos.coords.latitude;
        const uLng = pos.coords.longitude;
        studentLat = uLat;
        studentLng = uLng;
        passengerGpsLat = uLat;
        passengerGpsLng = uLng;
        evaluateCoLocation();

        // Compare student GPS with all stops on the route
        let minStop = null;
        let minDistance = Infinity;

        selectedRoute.stops.forEach((s) => {
          const sLat = parseFloat(s.lat);
          const sLng = parseFloat(s.lng);
          const d = haversineDistance(uLat, uLng, sLat, sLng);
          if (d < minDistance) {
            minDistance = d;
            minStop = s;
          }
        });

        if (!minStop) return;

        // Render / update Student "You Are Here" marker
        if (!studentMarker) {
          studentMarker = L.marker([uLat, uLng], { icon: makeStudentIcon(), zIndexOffset: 1200 })
            .addTo(map)
            .bindPopup(`<b>You are here</b><br>${formatDistance(minDistance)} to ${minStop.name || 'nearest stop'}`);
        } else {
          studentMarker.setLatLng([uLat, uLng]);
          studentMarker.setPopupContent(`<b>You are here</b><br>${formatDistance(minDistance)} to ${minStop.name || 'nearest stop'}`);
        }

        // Highlight nearest stop in the sidebar stop list
        document.querySelectorAll('.stop-item.is-nearest').forEach(el => {
          el.classList.remove('is-nearest');
          const b = el.querySelector('.badge-nearest');
          if (b) b.remove();
        });

        const stopItems = document.querySelectorAll('#detail-stop-list .stop-item');
        const stopIdx = selectedRoute.stops.indexOf(minStop);
        if (stopIdx >= 0 && stopItems[stopIdx]) {
          const targetItem = stopItems[stopIdx];
          targetItem.classList.add('is-nearest');
          const badge = document.createElement('span');
          badge.className = 'badge-nearest';
          badge.textContent = `Nearest • ${formatDistance(minDistance)}`;
          const infoContainer = targetItem.querySelector('div:nth-child(2)');
          if (infoContainer) infoContainer.appendChild(badge);
          targetItem.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        }

        // Update nearest stop banner
        const banner = document.getElementById('nearest-stop-banner');
        if (banner) {
          document.getElementById('nearest-stop-name').textContent = minStop.name || 'Stop';
          document.getElementById('nearest-stop-dist').textContent = formatDistance(minDistance);
          banner.classList.remove('hidden');
        }

        // Draw initial straight walking polyline
        const sLat = parseFloat(minStop.lat);
        const sLng = parseFloat(minStop.lng);

        if (walkingPolyline) map.removeLayer(walkingPolyline);
        walkingPolyline = L.polyline([[uLat, uLng], [sLat, sLng]], {
          color: '#10b981',
          weight: 4,
          opacity: 0.95,
          dashArray: '6 8'
        }).addTo(map);

        // Fetch realistic sidewalk path via OSRM walking profile
        try {
          const res = await fetch(`https://router.project-osrm.org/route/v1/walking/${uLng},${uLat};${sLng},${sLat}?overview=full&geometries=geojson`);
          const wData = await res.json();
          if (wData.code === 'Ok' && wData.routes && wData.routes[0]) {
            const walkRoute = wData.routes[0];
            const walkCoords = walkRoute.geometry.coordinates.map(c => [c[1], c[0]]);
            if (walkingPolyline) map.removeLayer(walkingPolyline);
            walkingPolyline = L.polyline(walkCoords, {
              color: '#10b981',
              weight: 4,
              opacity: 0.95,
              dashArray: '6 8'
            }).addTo(map);

            const realDist = walkRoute.distance;
            document.getElementById('nearest-stop-dist').textContent = formatDistance(realDist);
          }
        } catch {}

        // Fit map bounds to view both student and nearest stop
        const bounds = L.latLngBounds([[uLat, uLng], [sLat, sLng]]);
        map.fitBounds(bounds, { padding: [70, 70], maxZoom: 16 });
      },
      (err) => {
        if (btn) btn.classList.remove('locating');
        if (mapBtn) mapBtn.classList.remove('locating');
        if (btnText) btnText.textContent = 'Locate Me';
        let msg = 'Could not retrieve your location.';
        if (err.code === 1) msg = 'Location access was denied. Please enable location permissions in your browser.';
        else if (err.code === 2) msg = 'Location unavailable. Please check your GPS or internet connection.';
        else if (err.code === 3) msg = 'Location request timed out.';
        alert(msg);
      },
      { enableHighAccuracy: true, timeout: 10000, maximumAge: 30000 }
    );
  }

  /* ── Locate Me Event Listeners ─── */
  const btnLocateMe = document.getElementById('btn-locate-me');
  if (btnLocateMe) btnLocateMe.addEventListener('click', locateNearestStop);

  const mapLocateBtnEl = document.getElementById('map-locate-btn');
  if (mapLocateBtnEl) mapLocateBtnEl.addEventListener('click', locateNearestStop);

  const btnCloseNearest = document.getElementById('btn-close-nearest');
  if (btnCloseNearest) {
    btnCloseNearest.addEventListener('click', (e) => {
      e.stopPropagation();
      clearNearestStop();
    });
  }

  const nearestBanner = document.getElementById('nearest-stop-banner');
  if (nearestBanner) {
    nearestBanner.addEventListener('click', () => {
      if (studentLat !== null && studentLng !== null && selectedRoute && selectedRoute.stops) {
        const stopName = document.getElementById('nearest-stop-name').textContent;
        const targetStop = selectedRoute.stops.find(s => s.name === stopName);
        if (targetStop) {
          const bounds = L.latLngBounds([[studentLat, studentLng], [parseFloat(targetStop.lat), parseFloat(targetStop.lng)]]);
          map.fitBounds(bounds, { padding: [70, 70], maxZoom: 16 });
        }
      }
    });
  }

  /* ── Back button ─── */
  document.getElementById('btn-back').addEventListener('click', () => {
    stopCoLocationWatcher();
    clearMap();
    if (pollTimer) { clearInterval(pollTimer); pollTimer = null; }
    selectedRoute = null;

    sectionDetail.classList.add('hidden');
    sectionRoutes.classList.remove('hidden');
    mapPlaceholder.classList.remove('hidden');
    if (mobileMapBar) mobileMapBar.classList.add('hidden');

    document.getElementById('bus-loc-coords').textContent = 'Waiting for GPS…';
    document.getElementById('bus-loc-time').textContent   = '—';
    loadRoutes();
  });

  /* ── Mobile view map / stops buttons ─── */
  const btnViewMap = document.getElementById('btn-view-map');
  if (btnViewMap) btnViewMap.addEventListener('click', () => setMobileView('map'));
  if (mobileBtnStops) mobileBtnStops.addEventListener('click', () => setMobileView('panel'));

  /* ── Refresh button ─── */
  document.getElementById('btn-refresh').addEventListener('click', loadRoutes);

  /* ── Logout ─── */
  async function handleLogout() {
    try { await apiCall('POST', '/auth/logout', {}); } catch {}
    localStorage.clear();
    window.location.href = '../index.html';
  }
  document.getElementById('btn-logout').addEventListener('click', handleLogout);
  if (mobileLogoutBtn) mobileLogoutBtn.addEventListener('click', handleLogout);

  /* ── Boot ─── */
  loadRoutes();

})();
