#include "kerberos_context.h"

Nan::Persistent<FunctionTemplate> KerberosContext::constructor_template;

KerberosContext::KerberosContext() : Nan::ObjectWrap() {
    state = NULL;
    server_state = NULL;
}

KerberosContext::~KerberosContext() {
}

KerberosContext* KerberosContext::New() {
  Nan::HandleScope scope;
  Local<Object> obj = Nan::New(constructor_template)->GetFunction()->NewInstance();
  KerberosContext *kerberos_context = Nan::ObjectWrap::Unwrap<KerberosContext>(obj);
  return kerberos_context;
}

NAN_METHOD(KerberosContext::New) {
  // Create code object
  KerberosContext *kerberos_context = new KerberosContext();
  // Wrap it
  kerberos_context->Wrap(info.This());
  // Return the object
  info.GetReturnValue().Set(info.This());
}

void KerberosContext::Initialize(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target) {
  // Grab the scope of the call from Node
  Nan::HandleScope scope;

  // Define a new function template
  Local<FunctionTemplate> t = Nan::New<v8::FunctionTemplate>(static_cast<NAN_METHOD((*))>(New));
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(Nan::New<String>("KerberosContext").ToLocalChecked());

  // Get prototype
  Local<ObjectTemplate> proto = t->PrototypeTemplate();

  // Getter for the response
  Nan::SetAccessor(proto, Nan::New<String>("response").ToLocalChecked(), KerberosContext::ResponseGetter);

  // Getter for the username
  Nan::SetAccessor(proto, Nan::New<String>("username").ToLocalChecked(), KerberosContext::UsernameGetter);

  // Getter for the targetname - server side only
  Nan::SetAccessor(proto, Nan::New<String>("targetname").ToLocalChecked(), KerberosContext::TargetnameGetter);

  Nan::SetAccessor(proto, Nan::New<String>("delegatedCredentialsCache").ToLocalChecked(), KerberosContext::DelegatedCredentialsCacheGetter);

  // Set persistent
  constructor_template.Reset(t);
 // NanAssignPersistent(constructor_template, t);

  // Set the symbol
  target->ForceSet(Nan::New<String>("KerberosContext").ToLocalChecked(), t->GetFunction());
}


// Response Setter / Getter
NAN_GETTER(KerberosContext::ResponseGetter) {
  gss_client_state *client_state;
  gss_server_state *server_state;

  // Unpack the object
  KerberosContext *context = Nan::ObjectWrap::Unwrap<KerberosContext>(info.This());

  // Response could come from client or server state...
  client_state = context->state;
  server_state = context->server_state;

  // If client state is in use, take response from there, otherwise from server
  char *response = client_state != NULL ? client_state->response :
	  server_state != NULL ? server_state->response : NULL;

  if(response == NULL) {
    info.GetReturnValue().Set(Nan::Null());
  } else {
    // Return the response
    info.GetReturnValue().Set(Nan::New<String>(response).ToLocalChecked());
  }
}

// username Getter
NAN_GETTER(KerberosContext::UsernameGetter) {
  // Unpack the object
  KerberosContext *context = Nan::ObjectWrap::Unwrap<KerberosContext>(info.This());

  gss_client_state *client_state = context->state;
  gss_server_state *server_state = context->server_state;

  // If client state is in use, take response from there, otherwise from server
  char *username = client_state != NULL ? client_state->username :
	  server_state != NULL ? server_state->username : NULL;

  if(username == NULL) {
    info.GetReturnValue().Set(Nan::Null());
  } else {
    info.GetReturnValue().Set(Nan::New<String>(username).ToLocalChecked());
  }
}

// targetname Getter - server side only
NAN_GETTER(KerberosContext::TargetnameGetter) {
  // Unpack the object
  KerberosContext *context = Nan::ObjectWrap::Unwrap<KerberosContext>(info.This());

  gss_server_state *server_state = context->server_state;

  char *targetname = server_state != NULL ? server_state->targetname : NULL;

  if(targetname == NULL) {
    info.GetReturnValue().Set(Nan::Null());
  } else {
    info.GetReturnValue().Set(Nan::New<String>(targetname).ToLocalChecked());
  }
}

// targetname Getter - server side only
NAN_GETTER(KerberosContext::DelegatedCredentialsCacheGetter) {
  // Unpack the object
  KerberosContext *context = Nan::ObjectWrap::Unwrap<KerberosContext>(info.This());

  gss_server_state *server_state = context->server_state;

  char *delegated_credentials_cache = server_state != NULL ? server_state->delegated_credentials_cache : NULL;

  if(delegated_credentials_cache == NULL) {
    info.GetReturnValue().Set(Nan::Null());
  } else {
    info.GetReturnValue().Set(Nan::New<String>(delegated_credentials_cache).ToLocalChecked());
  }
}
