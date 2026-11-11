const API_HOST = window.location.hostname || 'localhost';
const API_BASE = window.location.protocol === 'https:'
  ? `${window.location.origin}/api`
  : `http://${API_HOST}:8080/api`;

async function apiCall(method, endpoint, body = null, customToken = null) {
  const opts = {
    method,
    headers: {
      'Content-Type':  'application/json',
      'Authorization': customToken !== null ? customToken : (localStorage.getItem('bus_token') || '')
    }
  };
  if (body) opts.body = JSON.stringify(body);
  const res = await fetch(API_BASE + endpoint, opts);
  if (res.status === 401 && !endpoint.includes('/auth/login') && !endpoint.includes('/auth/register')) {
    localStorage.removeItem('bus_token');
    const isSubdir = window.location.pathname.includes('/driver/') || window.location.pathname.includes('/passenger/');
    window.location.href = isSubdir ? '../index.html' : 'index.html';
    return { error: 'Session expired. Please sign in again.' };
  }
  const text = await res.text();
  try {
    return JSON.parse(text);
  } catch {
    return { error: res.ok ? 'Invalid server response.' : `Request failed (${res.status})` };
  }
}
