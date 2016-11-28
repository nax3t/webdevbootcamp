var kerberos = require('../build/Release/kerberos')
  , KerberosNative = kerberos.Kerberos;

var Kerberos = function() {
  this._native_kerberos = new KerberosNative(); 
}

// callback takes two arguments, an error string if defined and a new context
// uri should be given as service@host.  Services are not always defined
// in a straightforward way.  Use 'HTTP' for SPNEGO / Negotiate authentication. 
Kerberos.prototype.authGSSClientInit = function(uri, flags, callback) {
  return this._native_kerberos.authGSSClientInit(uri, flags, callback);
}

// This will obtain credentials using a credentials cache. To override the default
// location (posible /tmp/krb5cc_nnnnnn, where nnnn is your numeric uid) use 
// the environment variable KRB5CNAME. 
// The credentials (suitable for using in an 'Authenticate: ' header, when prefixed
// with 'Negotiate ') will be available as context.response inside the callback
// if no error is indicated.
// callback takes one argument, an error string if defined
Kerberos.prototype.authGSSClientStep = function(context, challenge, callback) {
  if(typeof challenge == 'function') {
    callback = challenge;
    challenge = '';
  }

  return this._native_kerberos.authGSSClientStep(context, challenge, callback);
}

Kerberos.prototype.authGSSClientUnwrap = function(context, challenge, callback) {
  if(typeof challenge == 'function') {
    callback = challenge;
    challenge = '';
  }

  return this._native_kerberos.authGSSClientUnwrap(context, challenge, callback);
}

Kerberos.prototype.authGSSClientWrap = function(context, challenge, user_name, callback) {
  if(typeof user_name == 'function') {
    callback = user_name;
    user_name = '';
  }

  return this._native_kerberos.authGSSClientWrap(context, challenge, user_name, callback);
}

// free memory used by a context created using authGSSClientInit.
// callback takes one argument, an error string if defined.
Kerberos.prototype.authGSSClientClean = function(context, callback) {
  return this._native_kerberos.authGSSClientClean(context, callback);
}

// The server will obtain credentials using a keytab.  To override the 
// default location (probably /etc/krb5.keytab) set the KRB5_KTNAME
// environment variable.
// The service name should be in the form service, or service@host.name
// e.g. for HTTP, use "HTTP" or "HTTP@my.host.name". See gss_import_name
// for GSS_C_NT_HOSTBASED_SERVICE.
//
// a boolean turns on "constrained_delegation". this enables acquisition of S4U2Proxy 
// credentials which will be stored in a credentials cache during the authGSSServerStep
// method. this parameter is optional.
//
// when "constrained_delegation" is enabled, a username can (optionally) be provided and
// S4U2Self protocol transition will be initiated. In this case, we will not
// require any "auth" data during the authGSSServerStep. This parameter is optional
// but constrained_delegation MUST be enabled for this to work. When S4U2Self is
// used, the username will be assumed to have been already authenticated, and no
// actual authentication will be performed. This is basically a way to "bootstrap"
// kerberos credentials (which can then be delegated with S4U2Proxy) for a user
// authenticated externally.
//
// callback takes two arguments, an error string if defined and a new context
//
Kerberos.prototype.authGSSServerInit = function(service, constrained_delegation, username, callback) {
  if(typeof(constrained_delegation) === 'function') {
	  callback = constrained_delegation;
	  constrained_delegation = false;
	  username = null;
  }

  if (typeof(constrained_delegation) === 'string') {
	  throw new Error("S4U2Self protocol transation is not possible without enabling constrained delegation");
  }

  if (typeof(username) === 'function') {
	  callback = username;
	  username = null;
  }

  constrained_delegation = !!constrained_delegation;
  
  return this._native_kerberos.authGSSServerInit(service, constrained_delegation, username, callback);
};

//callback takes one argument, an error string if defined.
Kerberos.prototype.authGSSServerClean = function(context, callback) {
  return this._native_kerberos.authGSSServerClean(context, callback);
};

// authData should be the base64 encoded authentication data obtained
// from client, e.g., in the Authorization header (without the leading 
// "Negotiate " string) during SPNEGO authentication.  The authenticated user 
// is available in context.username after successful authentication.
// callback takes one argument, an error string if defined.
//
// Note: when S4U2Self protocol transition was requested in the authGSSServerInit
// no actual authentication will be performed and authData will be ignored.
//
Kerberos.prototype.authGSSServerStep = function(context, authData, callback) {
  return this._native_kerberos.authGSSServerStep(context, authData, callback);
};

Kerberos.prototype.acquireAlternateCredentials = function(user_name, password, domain) {
  return this._native_kerberos.acquireAlternateCredentials(user_name, password, domain); 
}

Kerberos.prototype.prepareOutboundPackage = function(principal, inputdata) {
  return this._native_kerberos.prepareOutboundPackage(principal, inputdata); 
}

Kerberos.prototype.decryptMessage = function(challenge) {
  return this._native_kerberos.decryptMessage(challenge);
}

Kerberos.prototype.encryptMessage = function(challenge) {
  return this._native_kerberos.encryptMessage(challenge); 
}

Kerberos.prototype.queryContextAttribute = function(attribute) {
  if(typeof attribute != 'number' && attribute != 0x00) throw new Error("Attribute not supported");
  return this._native_kerberos.queryContextAttribute(attribute);
}

// Some useful result codes
Kerberos.AUTH_GSS_CONTINUE     = 0;
Kerberos.AUTH_GSS_COMPLETE     = 1;
     
// Some useful gss flags 
Kerberos.GSS_C_DELEG_FLAG      = 1;
Kerberos.GSS_C_MUTUAL_FLAG     = 2;
Kerberos.GSS_C_REPLAY_FLAG     = 4;
Kerberos.GSS_C_SEQUENCE_FLAG   = 8;
Kerberos.GSS_C_CONF_FLAG       = 16; 
Kerberos.GSS_C_INTEG_FLAG      = 32;
Kerberos.GSS_C_ANON_FLAG       = 64;
Kerberos.GSS_C_PROT_READY_FLAG = 128; 
Kerberos.GSS_C_TRANS_FLAG      = 256;

// Export Kerberos class
exports.Kerberos = Kerberos;

// If we have SSPI (windows)
if(kerberos.SecurityCredentials) {
  // Put all SSPI classes in it's own namespace
  exports.SSIP = {
      SecurityCredentials: require('./win32/wrappers/security_credentials').SecurityCredentials
    , SecurityContext: require('./win32/wrappers/security_context').SecurityContext
    , SecurityBuffer: require('./win32/wrappers/security_buffer').SecurityBuffer
    , SecurityBufferDescriptor: require('./win32/wrappers/security_buffer_descriptor').SecurityBufferDescriptor
  }
}
