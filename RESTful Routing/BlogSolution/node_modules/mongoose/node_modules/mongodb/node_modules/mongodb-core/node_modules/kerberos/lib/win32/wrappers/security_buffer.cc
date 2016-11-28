#include <node.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <v8.h>
#include <node_buffer.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "security_buffer.h"

using namespace node;

Nan::Persistent<FunctionTemplate> SecurityBuffer::constructor_template;

SecurityBuffer::SecurityBuffer(uint32_t security_type, size_t size) : Nan::ObjectWrap() {
  this->size = size;
  this->data = calloc(size, sizeof(char));
  this->security_type = security_type;
  // Set up the data in the sec_buffer
  this->sec_buffer.BufferType = security_type;
  this->sec_buffer.cbBuffer = (unsigned long)size;
  this->sec_buffer.pvBuffer = this->data;
}

SecurityBuffer::SecurityBuffer(uint32_t security_type, size_t size, void *data) : Nan::ObjectWrap() {
  this->size = size;
  this->data = data;
  this->security_type = security_type;
  // Set up the data in the sec_buffer
  this->sec_buffer.BufferType = security_type;
  this->sec_buffer.cbBuffer = (unsigned long)size;
  this->sec_buffer.pvBuffer = this->data;
}

SecurityBuffer::~SecurityBuffer() {
  free(this->data);
}

NAN_METHOD(SecurityBuffer::New) {
  SecurityBuffer *security_obj;

  if(info.Length() != 2)
    return Nan::ThrowError("Two parameters needed integer buffer type and  [32 bit integer/Buffer] required");

  if(!info[0]->IsInt32())
    return Nan::ThrowError("Two parameters needed integer buffer type and  [32 bit integer/Buffer] required");

  if(!info[1]->IsInt32() && !Buffer::HasInstance(info[1]))
    return Nan::ThrowError("Two parameters needed integer buffer type and  [32 bit integer/Buffer] required");

  // Unpack buffer type
  uint32_t buffer_type = info[0]->ToUint32()->Value();

  // If we have an integer
  if(info[1]->IsInt32()) {
    security_obj = new SecurityBuffer(buffer_type, info[1]->ToUint32()->Value());
  } else {
    // Get the length of the Buffer
    size_t length = Buffer::Length(info[1]->ToObject());
    // Allocate space for the internal void data pointer
    void *data = calloc(length, sizeof(char));
    // Write the data to out of V8 heap space
    memcpy(data, Buffer::Data(info[1]->ToObject()), length);
    // Create new SecurityBuffer
    security_obj = new SecurityBuffer(buffer_type, length, data);
  }

  // Wrap it
  security_obj->Wrap(info.This());
  // Return the object
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(SecurityBuffer::ToBuffer) {
  // Unpack the Security Buffer object
  SecurityBuffer *security_obj = Nan::ObjectWrap::Unwrap<SecurityBuffer>(info.This());
  // Create a Buffer
  Local<Object> buffer = Nan::CopyBuffer((char *)security_obj->data, (uint32_t)security_obj->size).ToLocalChecked();
  // Return the buffer
  info.GetReturnValue().Set(buffer);
}

void SecurityBuffer::Initialize(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target) {
  // Define a new function template
  Local<FunctionTemplate> t = Nan::New<FunctionTemplate>(New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(Nan::New<String>("SecurityBuffer").ToLocalChecked());

  // Class methods
  Nan::SetPrototypeMethod(t, "toBuffer", ToBuffer);

  // Set persistent
  constructor_template.Reset(t);

  // Set the symbol
  target->ForceSet(Nan::New<String>("SecurityBuffer").ToLocalChecked(), t->GetFunction());
}
