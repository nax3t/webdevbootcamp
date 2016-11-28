#ifndef SECURITY_BUFFER_DESCRIPTOR_H
#define SECURITY_BUFFER_DESCRIPTOR_H

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

#include <WinSock2.h>
#include <windows.h>
#include <sspi.h>
#include <nan.h>

using namespace v8;
using namespace node;

class SecurityBufferDescriptor : public Nan::ObjectWrap {
  public:
    Local<Array> arrayObject;
    SecBufferDesc secBufferDesc;

    SecurityBufferDescriptor();
    SecurityBufferDescriptor(const Nan::Persistent<Array>& arrayObjectPersistent);
    ~SecurityBufferDescriptor();

    // Has instance check
    static inline bool HasInstance(Local<Value> val) {
      if (!val->IsObject()) return false;
      Local<Object> obj = val->ToObject();
      return Nan::New(constructor_template)->HasInstance(obj);
    };

    char *toBuffer();
    size_t bufferSize();

    // Functions available from V8
    static void Initialize(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target);
    static NAN_METHOD(ToBuffer);

    // Constructor used for creating new Long objects from C++
    static Nan::Persistent<FunctionTemplate> constructor_template;

  private:
    static NAN_METHOD(New);
};

#endif