function getToken() { return localStorage.getItem('bus_token'); }
function getRole()  { return localStorage.getItem('bus_role');  }
function getName()  { return localStorage.getItem('bus_name');  }
function getId()    { return localStorage.getItem('bus_id');    }

function clearAuth() {
  ['bus_token','bus_role','bus_name','bus_id'].forEach(k => localStorage.removeItem(k));
}
