#ifndef KERBEROS_CONTEXT_H
#define KERBEROS_CONTEXT_H

#include <node.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h>

#include "nan.h"
#include <node_object_wrap.h>
#include <v8.h>

extern "C" {
  #include "kerberosgss.h"
}

using namespace v8;
using namespace node;

class KerberosContext : public Nan::ObjectWrap {

public:
  KerberosContext();
  ~KerberosContext();

  static inline bool HasInstance(Local<Value> val) {
    if (!val->IsObject()) return false;
    Local<Object> obj = val->ToObject();
    return Nan::New(constructor_template)->HasInstance(obj);
  };

  inline bool IsClientInstance() {
      return state != NULL;
  }

  inline bool IsServerInstance() {
      return server_state != NULL;
  }

  // Constructor used for creating new Kerberos objects from C++
  static Nan::Persistent<FunctionTemplate> constructor_template;

  // Initialize function for the object
  static void Initialize(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target);

  // Public constructor
  static KerberosContext* New();

  // Handle to the kerberos client context
  gss_client_state *state;

  // Handle to the kerberos server context
  gss_server_state *server_state;

private:
  static NAN_METHOD(New);
  // In either client state or server state
  static NAN_GETTER(ResponseGetter);
  static NAN_GETTER(UsernameGetter);
  // Only in the "server_state"
  static NAN_GETTER(TargetnameGetter);
  static NAN_GETTER(DelegatedCredentialsCacheGetter);
};
#endif
