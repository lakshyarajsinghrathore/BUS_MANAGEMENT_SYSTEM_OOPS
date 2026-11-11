/* ── driver.js ──────────────────────────────────────────
   Handles: map init, click-to-add-stop, start/end ride,
            GPS watchPosition → POST /api/location/update
   ──────────────────────────────────────────────────── */

(function () {
  /* ── Auth guard ─── */
  const token = localStorage.getItem('bus_token');
  const role  = localStorage.getItem('bus_role');
  const name  = localStorage.getItem('bus_name');
  const uid   = localStorage.getItem('bus_id');

  if (!token || role !== 'driver') {
    window.location.href = '../index.html';
    return;
  }

  /* ── Populate user info ─── */
  const safeName = (name || '').trim() || 'Driver';
  document.getElementById('user-name').textContent   = safeName;
  document.getElementById('user-avatar').textContent = safeName[0].toUpperCase();

  function escapeHtml(str) {
    return String(str ?? '').replace(/[&<>"']/g, ch => ({
      '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    }[ch]));
  }

  /* ── Mobile view switcher ─── */
  const mobileTabBtns = document.querySelectorAll('.mobile-tab-btn[data-mobile-tab]');
  const mobileStopsCount = document.getElementById('mobile-stops-count');
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
  const map = L.map('map', { zoomControl: true }).setView([24.5854, 73.7125], 13);

  L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
    attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors',
    maxZoom: 19
  }).addTo(map);


  /* ── State ─── */
  let stops         = [];   // [{lat, lng, name}]
  let stopMarkers   = [];   // Leaflet markers
  let routePolyline = null;
  let gpsMarker     = null;
  let watchId       = null;
  let rideActive    = false;
  let gpsInterval   = null;
  let lastLat = null, lastLng = null;

  /* ── DOM refs ─── */
  const stopList        = document.getElementById('stop-list');
  const activeStopList  = document.getElementById('active-stop-list');
  const stopCountBadge  = document.getElementById('stop-count-badge');
  const stateSetup      = document.getElementById('state-setup');
  const stateActive     = document.getElementById('state-active');
  const mapHint         = document.getElementById('map-hint');
  const mapOverlay      = document.getElementById('map-overlay');
  const overlayCoords   = document.getElementById('overlay-coords');
  const overlayTime     = document.getElementById('overlay-time');
  const templateSelect  = document.getElementById('template-select');
  const btnDeleteTemplate = document.getElementById('btn-delete-template');
  const saveTemplateCheck = document.getElementById('save-template-check');
  let availableTemplates = [];

  /* ── Numbered marker factory ─── */
  function makeStopIcon(n) {
    return L.divIcon({
      className: '',
      html: `<div class="stop-marker-icon">${n}</div>`,
      iconSize: [28, 28],
      iconAnchor: [14, 14]
    });
  }

  /* ── GPS marker factory ─── */
  function makeGpsIcon() {
    return L.divIcon({
      className: '',
      html: `<div class="gps-dot-outer"><div class="gps-dot-inner"></div></div>`,
      iconSize: [20, 20],
      iconAnchor: [10, 10]
    });
  }

  /* ── Render stop list in sidebar ─── */
  function renderStopList() {
    stopList.innerHTML = '';
    stops.forEach((s, i) => {
      const div = document.createElement('div');
      div.className = 'stop-item';
      div.innerHTML = `
        <div class="stop-num">${i + 1}</div>
        <div style="flex:1;min-width:0;">
          <div class="stop-name">${escapeHtml(s.name)}</div>
          <div class="stop-coords">${s.lat.toFixed(5)}, ${s.lng.toFixed(5)}</div>
        </div>
        <button class="stop-del" data-idx="${i}" title="Remove" aria-label="Remove stop ${i + 1}">
          <svg viewBox="0 0 20 20" fill="currentColor"><path fill-rule="evenodd" d="M4.293 4.293a1 1 0 011.414 0L10 8.586l4.293-4.293a1 1 0 111.414 1.414L11.414 10l4.293 4.293a1 1 0 01-1.414 1.414L10 11.414l-4.293 4.293a1 1 0 01-1.414-1.414L8.586 10 4.293 5.707a1 1 0 010-1.414z" clip-rule="evenodd"/></svg>
        </button>`;
      stopList.appendChild(div);
    });
    stopCountBadge.textContent = `${stops.length} added`;
    if (mobileStopsCount) mobileStopsCount.textContent = stops.length;
    updatePolyline();
  }

  /* ── Delete a stop ─── */
  stopList.addEventListener('click', (e) => {
    const btn = e.target.closest('.stop-del');
    if (!btn) return;
    const idx = parseInt(btn.dataset.idx);
    map.removeLayer(stopMarkers[idx]);
    stops.splice(idx, 1);
    stopMarkers.splice(idx, 1);
    stopMarkers.forEach((m, i) => m.setIcon(makeStopIcon(i + 1)));

    // Remove polyline if less than 2 stops remain
    if (stops.length < 2 && routePolyline) {
      map.removeLayer(routePolyline);
      routePolyline = null;
    }

    renderStopList();
  });

  /* ── Route segment by segment to avoid OSRM detours ─── */
  async function fetchRoadPath(stops) {
    const allLatLng = [];
    for (let i = 0; i < stops.length - 1; i++) {
      const a = stops[i], b = stops[i + 1];
      try {
        const res  = await fetch(`https://router.project-osrm.org/route/v1/driving/${a.lng},${a.lat};${b.lng},${b.lat}?overview=full&geometries=geojson`);
        const data = await res.json();
        if (data.code === 'Ok' && data.routes[0]) {
          const segCoords = data.routes[0].geometry.coordinates;
          // Drop first point of each segment (except first) to avoid duplicates at junctions
          const seg = segCoords.map(c => [c[1], c[0]]);
          if (allLatLng.length) seg.shift();
          allLatLng.push(...seg);
          continue;
        }
      } catch {}
      // Fallback straight segment
      if (!allLatLng.length) allLatLng.push([a.lat, a.lng]);
      allLatLng.push([b.lat, b.lng]);
    }
    return allLatLng;
  }

  let polylineSeq = 0;

  async function updatePolyline() {
    const seq = ++polylineSeq;

    if (routePolyline) { map.removeLayer(routePolyline); routePolyline = null; }
    if (stops.length < 2) return;

    const currentStops = [...stops]; // Snapshot stops to detect changes
    const latLngs = await fetchRoadPath(currentStops);

    // Only draw if this is still the latest request and stops haven't changed
    if (seq === polylineSeq && stops.length >= 2) {
      routePolyline = L.polyline(
        latLngs.length >= 2 ? latLngs : currentStops.map(s => [s.lat, s.lng]),
        { color: '#ef4444', weight: 4, opacity: 0.9 }
      ).addTo(map);
    }
  }

  /* ── Map click → add stop ─── */
  map.on('click', (e) => {
    if (rideActive) return;
    const { lat, lng } = e.latlng;
    const n = stops.length + 1;
    const stopName = `Stop ${n}`;
    stops.push({ lat, lng, name: stopName });
    const marker = L.marker([lat, lng], { icon: makeStopIcon(n) })
      .addTo(map)
      .bindPopup(`<b>${escapeHtml(stopName)}</b><br>${lat.toFixed(5)}, ${lng.toFixed(5)}`);
    stopMarkers.push(marker);
    renderStopList();
  });

  /* ── Route Templates ─── */
  async function loadTemplates() {
    try {
      const data = await apiCall('GET', '/templates', null, token);
      if (Array.isArray(data)) {
        availableTemplates = data;
        templateSelect.innerHTML = '<option value="">-- Custom Route (Click Map) --</option>';
        data.forEach(t => {
          const opt = document.createElement('option');
          opt.value = t.id;
          opt.textContent = `${t.name} (${t.stop_count} stops)`;
          templateSelect.appendChild(opt);
        });
      }
    } catch (err) {
      console.warn('Could not load templates:', err);
    }
  }

  templateSelect.addEventListener('change', async () => {
    const tid = templateSelect.value;
    if (btnDeleteTemplate) btnDeleteTemplate.style.display = tid ? 'flex' : 'none';
    if (!tid) return;

    const tmpl = availableTemplates.find(t => t.id == tid);
    if (tmpl) {
      document.getElementById('bus-number').value = tmpl.bus_number || 'BUS-01';
      document.getElementById('route-name').value = tmpl.name;
    }

    try {
      const data = await apiCall('GET', `/templates/${tid}`, null, token);
      if (Array.isArray(data) && data.length) {
        // Clear previous stops
        stopMarkers.forEach(m => map.removeLayer(m));
        stopMarkers = [];
        stops = data.map(s => ({ lat: s.lat, lng: s.lng, name: s.name }));

        // Plot markers
        stops.forEach((s, i) => {
          const m = L.marker([s.lat, s.lng], { icon: makeStopIcon(i + 1) })
            .addTo(map)
            .bindPopup(`<b>${escapeHtml(s.name)}</b><br>${s.lat.toFixed(5)}, ${s.lng.toFixed(5)}`);
          stopMarkers.push(m);
        });

        renderStopList();
        map.fitBounds(stops.map(s => [s.lat, s.lng]), { padding: [40, 40] });
      }
    } catch (err) {
      console.warn('Failed to load template stops:', err);
    }
  });

  /* ── Delete template button ─── */
  if (btnDeleteTemplate) {
    btnDeleteTemplate.addEventListener('click', async () => {
      const tid = templateSelect.value;
      if (!tid) return;

      const tmpl = availableTemplates.find(t => t.id == tid);
      const name = tmpl ? tmpl.name : 'this route template';

      // Alert confirmation popup
      if (!confirm(`Are you sure you want to delete the saved template "${name}"?`)) {
        return;
      }

      try {
        const res = await apiCall('DELETE', `/templates/${tid}`);
        if (res && res.error) {
          alert('Could not delete template: ' + res.error);
          return;
        }

        // Reset UI
        templateSelect.value = '';
        btnDeleteTemplate.style.display = 'none';
        document.getElementById('bus-number').value = '';
        document.getElementById('route-name').value = '';
        stopMarkers.forEach(m => map.removeLayer(m));
        stopMarkers = [];
        stops = [];
        if (routePolyline) { map.removeLayer(routePolyline); routePolyline = null; }
        renderStopList();

        // Reload templates list
        await loadTemplates();
      } catch (err) {
        alert('Failed to delete template: network error.');
      }
    });
  }

  /* ── Clear stops button ─── */
  document.getElementById('btn-clear-stops').addEventListener('click', () => {
    if (rideActive) return;
    templateSelect.value = '';
    if (btnDeleteTemplate) btnDeleteTemplate.style.display = 'none';
    stopMarkers.forEach(m => map.removeLayer(m));
    stopMarkers = [];
    stops = [];
    if (routePolyline) { map.removeLayer(routePolyline); routePolyline = null; }
    renderStopList();
  });

  /* ── Start Ride ─── */
  document.getElementById('btn-start-ride').addEventListener('click', async () => {
    const busNum   = document.getElementById('bus-number').value.trim();
    const routeName= document.getElementById('route-name').value.trim();

    if (!busNum)       { alert('Please enter a bus number.'); return; }
    if (!routeName)    { alert('Please enter a route name.'); return; }
    if (stops.length < 2) { alert('Add at least 2 stops on the map.'); return; }

    const payload = {
      bus_number: busNum,
      route_name: routeName,
      save_as_template: !!(saveTemplateCheck && saveTemplateCheck.checked),
      stops: stops.map((s, i) => ({
        name: s.name,
        lat: s.lat,
        lng: s.lng,
        stop_order: i + 1,
        order: i + 1
      }))
    };

    try {
      const res = await apiCall('POST', '/route/create', payload, token);
      if (res.error) { alert('Error: ' + res.error); return; }
      if (saveTemplateCheck && saveTemplateCheck.checked) {
        saveTemplateCheck.checked = false;
        loadTemplates();
      }
    } catch {
      alert('Could not reach server. Starting offline preview.');
    }

    /* switch UI to active state */
    rideActive = true;
    stateSetup.classList.add('hidden');
    stateActive.classList.remove('hidden');
    mapHint.classList.add('hidden');
    mapOverlay.classList.remove('hidden');

    document.getElementById('active-bus').textContent   = busNum;
    document.getElementById('active-route').textContent = routeName;
    document.getElementById('active-stops').textContent = stops.length;

    /* render read-only stop list */
    activeStopList.innerHTML = '';
    stops.forEach((s, i) => {
      const div = document.createElement('div');
      div.className = 'stop-item';
      div.innerHTML = `
        <div class="stop-num">${i + 1}</div>
        <div>
          <div class="stop-name">${escapeHtml(s.name)}</div>
          <div class="stop-coords">${s.lat.toFixed(5)}, ${s.lng.toFixed(5)}</div>
        </div>`;
      activeStopList.appendChild(div);
    });

    if (window.innerWidth <= 768) {
      setMobileView('map');
    }

    /* redraw route segment-by-segment (reuses fetchRoadPath for accuracy) */
    if (routePolyline) map.removeLayer(routePolyline);
    const latLngs = await fetchRoadPath(stops);
    routePolyline = L.polyline(
      latLngs.length >= 2 ? latLngs : stops.map(s => [s.lat, s.lng]),
      { color: '#ef4444', weight: 4, opacity: 0.95 }
    ).addTo(map);

    startGPS();
    startOccupancyTracker();
  });

  /* ── GPS tracking ─── */
  function startGPS() {
    if (!navigator.geolocation) {
      document.getElementById('gps-status').innerHTML =
        '<span style="color:var(--danger);font-size:0.78rem">GPS unavailable</span>';
      return;
    }

    watchId = navigator.geolocation.watchPosition(
      (pos) => {
        lastLat = pos.coords.latitude;
        lastLng = pos.coords.longitude;
        updateGpsUI(lastLat, lastLng);
        sendLocation(lastLat, lastLng);
      },
      (err) => console.warn('GPS error:', err),
      { enableHighAccuracy: true, maximumAge: 5000, timeout: 15000 }
    );
  }

  function updateGpsUI(lat, lng) {
    overlayCoords.textContent = `${lat.toFixed(6)}, ${lng.toFixed(6)}`;
    overlayTime.textContent   = new Date().toLocaleTimeString();

    if (!gpsMarker) {
      gpsMarker = L.marker([lat, lng], { icon: makeGpsIcon(), zIndexOffset: 1000 })
        .addTo(map)
        .bindPopup('<b>Your location</b>');
      map.setView([lat, lng], 15);
    } else {
      gpsMarker.setLatLng([lat, lng]);
    }
  }

  async function sendLocation(lat, lng) {
    try {
      await apiCall('POST', '/location/update', { lat, lng }, token);
    } catch { /* silent — GPS still works visually */ }
  }

  /* ── Live Occupancy Tracking for Driver HUD ─── */
  let occupancyTimer = null;

  async function updateDriverOccupancy() {
    if (!rideActive || !uid) return;
    try {
      const data = await apiCall('GET', `/location/${uid}`);
      if (data && data.occupancy) {
        const occ = data.occupancy;
        const occText = `${occ.occupied_seats} / ${occ.capacity} (${Math.round(occ.occupancy_percent)}% full)`;
        const overlayRiders = document.getElementById('overlay-riders');
        if (overlayRiders) overlayRiders.textContent = occText;

        const activeOccText = document.getElementById('active-occupancy-text');
        if (activeOccText) activeOccText.textContent = occText;

        const activeOccBadge = document.getElementById('active-occupancy-badge');
        if (activeOccBadge) {
          if (occ.available_percent <= 15) {
            activeOccBadge.style.background = 'rgba(239, 68, 68, 0.15)';
            activeOccBadge.style.color = '#ef4444';
            activeOccBadge.textContent = occ.available_seats === 0 ? 'Full' : 'Standing';
          } else if (occ.available_percent <= 50) {
            activeOccBadge.style.background = 'rgba(245, 158, 11, 0.15)';
            activeOccBadge.style.color = '#f59e0b';
            activeOccBadge.textContent = 'Filling Up';
          } else {
            activeOccBadge.style.background = 'rgba(16, 185, 129, 0.15)';
            activeOccBadge.style.color = '#10b981';
            activeOccBadge.textContent = 'Available';
          }
        }
      }
    } catch {}
  }

  function startOccupancyTracker() {
    if (occupancyTimer) clearInterval(occupancyTimer);
    updateDriverOccupancy();
    occupancyTimer = setInterval(updateDriverOccupancy, 5000);
  }

  function stopOccupancyTracker() {
    if (occupancyTimer) {
      clearInterval(occupancyTimer);
      occupancyTimer = null;
    }
  }

  /* ── End Ride ─── */
  document.getElementById('btn-end-ride').addEventListener('click', async () => {
    if (!confirm('End this ride? Passengers will no longer see the bus.')) return;

    try {
      const res = await apiCall('PUT', '/route/deactivate', {}, token);
      if (res.error) {
        alert('Could not end ride: ' + res.error);
        return;
      }
    } catch (err) {
      alert('Could not reach server to end ride.');
      return;
    }

    stopOccupancyTracker();

    if (watchId !== null) {
      navigator.geolocation.clearWatch(watchId);
      watchId = null;
    }
    if (gpsInterval) { clearInterval(gpsInterval); gpsInterval = null; }

    /* clean up map */
    if (gpsMarker)    { map.removeLayer(gpsMarker);    gpsMarker = null; }
    if (routePolyline){ map.removeLayer(routePolyline); routePolyline = null; }
    stopMarkers.forEach(m => map.removeLayer(m));
    stopMarkers = [];
    stops = [];

    rideActive = false;
    stateActive.classList.add('hidden');
    stateSetup.classList.remove('hidden');
    mapHint.classList.remove('hidden');
    mapOverlay.classList.add('hidden');

    document.getElementById('bus-number').value  = '';
    document.getElementById('route-name').value  = '';
    renderStopList();
    if (window.innerWidth <= 768) {
      setMobileView('panel');
    }
  });

  /* ── Pick stops on map CTA (mobile) ─── */
  const btnPickStopsMap = document.getElementById('btn-pick-stops-map');
  if (btnPickStopsMap) {
    btnPickStopsMap.addEventListener('click', () => setMobileView('map'));
  }

  /* ── Logout ─── */
  async function handleLogout() {
    try { await apiCall('POST', '/auth/logout', {}, token); } catch {}
    localStorage.clear();
    window.location.href = '../index.html';
  }
  document.getElementById('btn-logout').addEventListener('click', handleLogout);
  if (mobileLogoutBtn) mobileLogoutBtn.addEventListener('click', handleLogout);

  /* ── Check for existing active ride on load ─── */
  async function checkActiveRide() {
    try {
      const res = await apiCall('GET', '/route/active', null, token);
      if (res.active) {
        // Restore state from server
        const busNum = res.bus_number || `BUS-${res.bus_id}`;
        const routeName = res.route_name;
        stops = res.stops.map(s => ({ lat: s.lat, lng: s.lng, name: s.name }));

        // Switch to active state UI
        rideActive = true;
        stateSetup.classList.add('hidden');
        stateActive.classList.remove('hidden');
        mapHint.classList.add('hidden');
        mapOverlay.classList.remove('hidden');

        document.getElementById('active-bus').textContent = busNum;
        document.getElementById('active-route').textContent = routeName;
        document.getElementById('active-stops').textContent = stops.length;
        if (mobileStopsCount) mobileStopsCount.textContent = stops.length;

        // Render read-only stop list
        activeStopList.innerHTML = '';
        stops.forEach((s, i) => {
          const div = document.createElement('div');
          div.className = 'stop-item';
          div.innerHTML = `
            <div class="stop-num">${i + 1}</div>
            <div>
              <div class="stop-name">${escapeHtml(s.name)}</div>
              <div class="stop-coords">${s.lat.toFixed(5)}, ${s.lng.toFixed(5)}</div>
            </div>`;
          activeStopList.appendChild(div);
        });

        // Draw stop markers
        stops.forEach((s, i) => {
          const marker = L.marker([s.lat, s.lng], { icon: makeStopIcon(i + 1) })
            .addTo(map)
            .bindPopup(`<b>${escapeHtml(s.name)}</b><br>${s.lat.toFixed(5)}, ${s.lng.toFixed(5)}`);
          stopMarkers.push(marker);
        });

        if (window.innerWidth <= 768) {
          setMobileView('map');
        }

        // Fit map to route bounds immediately
        if (stops.length) {
          const bounds = L.latLngBounds(stops.map(s => [s.lat, s.lng]));
          setTimeout(() => {
            map.invalidateSize();
            map.fitBounds(bounds, { padding: [50, 50] });
          }, 80);
        }

        // Immediately draw line and start GPS so driver doesn't wait
        if (stops.length >= 2) {
          routePolyline = L.polyline(stops.map(s => [s.lat, s.lng]), {
            color: '#ef4444',
            weight: 4,
            opacity: 0.95
          }).addTo(map);
        }
        startGPS();
        startOccupancyTracker();

        // Refine road path polyline in background
        if (stops.length >= 2) {
          fetchRoadPath(stops).then(latLngs => {
            if (rideActive && latLngs && latLngs.length >= 2) {
              if (routePolyline) map.removeLayer(routePolyline);
              routePolyline = L.polyline(latLngs, {
                color: '#ef4444',
                weight: 4,
                opacity: 0.95
              }).addTo(map);
            }
          }).catch(() => {});
        }
      }
    } catch (err) {
      console.warn('Could not check for active ride:', err);
    }
  }

  checkActiveRide();
  loadTemplates();

})();
