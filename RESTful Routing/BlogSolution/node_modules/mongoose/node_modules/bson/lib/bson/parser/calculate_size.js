"use strict"

var writeIEEE754 = require('../float_parser').writeIEEE754
	, readIEEE754 = require('../float_parser').readIEEE754
	, Long = require('../long').Long
  , Double = require('../double').Double
  , Timestamp = require('../timestamp').Timestamp
  , ObjectID = require('../objectid').ObjectID
  , Symbol = require('../symbol').Symbol
  , BSONRegExp = require('../regexp').BSONRegExp
  , Code = require('../code').Code
  , MinKey = require('../min_key').MinKey
  , MaxKey = require('../max_key').MaxKey
  , DBRef = require('../db_ref').DBRef
  , Binary = require('../binary').Binary;

// To ensure that 0.4 of node works correctly
var isDate = function isDate(d) {
  return typeof d === 'object' && Object.prototype.toString.call(d) === '[object Date]';
}

var calculateObjectSize = function calculateObjectSize(object, serializeFunctions, ignoreUndefined) {
  var totalLength = (4 + 1);

  if(Array.isArray(object)) {
    for(var i = 0; i < object.length; i++) {
      totalLength += calculateElement(i.toString(), object[i], serializeFunctions, true, ignoreUndefined)
    }
  } else {
		// If we have toBSON defined, override the current object
		if(object.toBSON) {
			object = object.toBSON();
		}

		// Calculate size
    for(var key in object) {
      totalLength += calculateElement(key, object[key], serializeFunctions, false, ignoreUndefined)
    }
  }

  return totalLength;
}

/**
 * @ignore
 * @api private
 */
function calculateElement(name, value, serializeFunctions, isArray, ignoreUndefined) {
	// If we have toBSON defined, override the current object
  if(value && value.toBSON){
    value = value.toBSON();
  }

  switch(typeof value) {
    case 'string':
      return 1 + Buffer.byteLength(name, 'utf8') + 1 + 4 + Buffer.byteLength(value, 'utf8') + 1;
    case 'number':
      if(Math.floor(value) === value && value >= BSON.JS_INT_MIN && value <= BSON.JS_INT_MAX) {
        if(value >= BSON.BSON_INT32_MIN && value <= BSON.BSON_INT32_MAX) { // 32 bit
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (4 + 1);
        } else {
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (8 + 1);
        }
      } else {  // 64 bit
        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (8 + 1);
      }
    case 'undefined':
      if(isArray || !ignoreUndefined) return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (1);
      return 0;
    case 'boolean':
      return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (1 + 1);
    case 'object':
      if(value == null || value instanceof MinKey || value instanceof MaxKey || value['_bsontype'] == 'MinKey' || value['_bsontype'] == 'MaxKey') {
        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (1);
      } else if(value instanceof ObjectID || value['_bsontype'] == 'ObjectID') {
        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (12 + 1);
      } else if(value instanceof Date || isDate(value)) {
        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (8 + 1);
      } else if(typeof Buffer !== 'undefined' && Buffer.isBuffer(value)) {
        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (1 + 4 + 1) + value.length;
      } else if(value instanceof Long || value instanceof Double || value instanceof Timestamp
          || value['_bsontype'] == 'Long' || value['_bsontype'] == 'Double' || value['_bsontype'] == 'Timestamp') {
        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (8 + 1);
      } else if(value instanceof Code || value['_bsontype'] == 'Code') {
        // Calculate size depending on the availability of a scope
        if(value.scope != null && Object.keys(value.scope).length > 0) {
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + 1 + 4 + 4 + Buffer.byteLength(value.code.toString(), 'utf8') + 1 + calculateObjectSize(value.scope, serializeFunctions, ignoreUndefined);
        } else {
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + 1 + 4 + Buffer.byteLength(value.code.toString(), 'utf8') + 1;
        }
      } else if(value instanceof Binary || value['_bsontype'] == 'Binary') {
        // Check what kind of subtype we have
        if(value.sub_type == Binary.SUBTYPE_BYTE_ARRAY) {
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (value.position + 1 + 4 + 1 + 4);
        } else {
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + (value.position + 1 + 4 + 1);
        }
      } else if(value instanceof Symbol || value['_bsontype'] == 'Symbol') {
        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + Buffer.byteLength(value.value, 'utf8') + 4 + 1 + 1;
      } else if(value instanceof DBRef || value['_bsontype'] == 'DBRef') {
        // Set up correct object for serialization
        var ordered_values = {
            '$ref': value.namespace
          , '$id' : value.oid
        };

        // Add db reference if it exists
        if(null != value.db) {
          ordered_values['$db'] = value.db;
        }

        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + 1 + calculateObjectSize(ordered_values, serializeFunctions, ignoreUndefined);
      } else if(value instanceof RegExp || Object.prototype.toString.call(value) === '[object RegExp]') {
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + 1 + Buffer.byteLength(value.source, 'utf8') + 1
            + (value.global ? 1 : 0) + (value.ignoreCase ? 1 : 0) + (value.multiline ? 1 : 0) + 1
      } else if(value instanceof BSONRegExp || value['_bsontype'] == 'BSONRegExp') {
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + 1 + Buffer.byteLength(value.pattern, 'utf8') + 1
            + Buffer.byteLength(value.options, 'utf8') + 1
      } else {
        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + calculateObjectSize(value, serializeFunctions, ignoreUndefined) + 1;
      }
    case 'function':
      // WTF for 0.4.X where typeof /someregexp/ === 'function'
      if(value instanceof RegExp || Object.prototype.toString.call(value) === '[object RegExp]' || String.call(value) == '[object RegExp]') {
        return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + 1 + Buffer.byteLength(value.source, 'utf8') + 1
          + (value.global ? 1 : 0) + (value.ignoreCase ? 1 : 0) + (value.multiline ? 1 : 0) + 1
      } else {
        if(serializeFunctions && value.scope != null && Object.keys(value.scope).length > 0) {
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + 1 + 4 + 4 + Buffer.byteLength(value.toString(), 'utf8') + 1 + calculateObjectSize(value.scope, serializeFunctions, ignoreUndefined);
        } else if(serializeFunctions) {
          return (name != null ? (Buffer.byteLength(name, 'utf8') + 1) : 0) + 1 + 4 + Buffer.byteLength(value.toString(), 'utf8') + 1;
        }
      }
  }

  return 0;
}

var BSON = {};

/**
 * Contains the function cache if we have that enable to allow for avoiding the eval step on each deserialization, comparison is by md5
 *
 * @ignore
 * @api private
 */
var functionCache = BSON.functionCache = {};

/**
 * Number BSON Type
 *
 * @classconstant BSON_DATA_NUMBER
 **/
BSON.BSON_DATA_NUMBER = 1;
/**
 * String BSON Type
 *
 * @classconstant BSON_DATA_STRING
 **/
BSON.BSON_DATA_STRING = 2;
/**
 * Object BSON Type
 *
 * @classconstant BSON_DATA_OBJECT
 **/
BSON.BSON_DATA_OBJECT = 3;
/**
 * Array BSON Type
 *
 * @classconstant BSON_DATA_ARRAY
 **/
BSON.BSON_DATA_ARRAY = 4;
/**
 * Binary BSON Type
 *
 * @classconstant BSON_DATA_BINARY
 **/
BSON.BSON_DATA_BINARY = 5;
/**
 * ObjectID BSON Type
 *
 * @classconstant BSON_DATA_OID
 **/
BSON.BSON_DATA_OID = 7;
/**
 * Boolean BSON Type
 *
 * @classconstant BSON_DATA_BOOLEAN
 **/
BSON.BSON_DATA_BOOLEAN = 8;
/**
 * Date BSON Type
 *
 * @classconstant BSON_DATA_DATE
 **/
BSON.BSON_DATA_DATE = 9;
/**
 * null BSON Type
 *
 * @classconstant BSON_DATA_NULL
 **/
BSON.BSON_DATA_NULL = 10;
/**
 * RegExp BSON Type
 *
 * @classconstant BSON_DATA_REGEXP
 **/
BSON.BSON_DATA_REGEXP = 11;
/**
 * Code BSON Type
 *
 * @classconstant BSON_DATA_CODE
 **/
BSON.BSON_DATA_CODE = 13;
/**
 * Symbol BSON Type
 *
 * @classconstant BSON_DATA_SYMBOL
 **/
BSON.BSON_DATA_SYMBOL = 14;
/**
 * Code with Scope BSON Type
 *
 * @classconstant BSON_DATA_CODE_W_SCOPE
 **/
BSON.BSON_DATA_CODE_W_SCOPE = 15;
/**
 * 32 bit Integer BSON Type
 *
 * @classconstant BSON_DATA_INT
 **/
BSON.BSON_DATA_INT = 16;
/**
 * Timestamp BSON Type
 *
 * @classconstant BSON_DATA_TIMESTAMP
 **/
BSON.BSON_DATA_TIMESTAMP = 17;
/**
 * Long BSON Type
 *
 * @classconstant BSON_DATA_LONG
 **/
BSON.BSON_DATA_LONG = 18;
/**
 * MinKey BSON Type
 *
 * @classconstant BSON_DATA_MIN_KEY
 **/
BSON.BSON_DATA_MIN_KEY = 0xff;
/**
 * MaxKey BSON Type
 *
 * @classconstant BSON_DATA_MAX_KEY
 **/
BSON.BSON_DATA_MAX_KEY = 0x7f;

/**
 * Binary Default Type
 *
 * @classconstant BSON_BINARY_SUBTYPE_DEFAULT
 **/
BSON.BSON_BINARY_SUBTYPE_DEFAULT = 0;
/**
 * Binary Function Type
 *
 * @classconstant BSON_BINARY_SUBTYPE_FUNCTION
 **/
BSON.BSON_BINARY_SUBTYPE_FUNCTION = 1;
/**
 * Binary Byte Array Type
 *
 * @classconstant BSON_BINARY_SUBTYPE_BYTE_ARRAY
 **/
BSON.BSON_BINARY_SUBTYPE_BYTE_ARRAY = 2;
/**
 * Binary UUID Type
 *
 * @classconstant BSON_BINARY_SUBTYPE_UUID
 **/
BSON.BSON_BINARY_SUBTYPE_UUID = 3;
/**
 * Binary MD5 Type
 *
 * @classconstant BSON_BINARY_SUBTYPE_MD5
 **/
BSON.BSON_BINARY_SUBTYPE_MD5 = 4;
/**
 * Binary User Defined Type
 *
 * @classconstant BSON_BINARY_SUBTYPE_USER_DEFINED
 **/
BSON.BSON_BINARY_SUBTYPE_USER_DEFINED = 128;

// BSON MAX VALUES
BSON.BSON_INT32_MAX = 0x7FFFFFFF;
BSON.BSON_INT32_MIN = -0x80000000;

BSON.BSON_INT64_MAX = Math.pow(2, 63) - 1;
BSON.BSON_INT64_MIN = -Math.pow(2, 63);

// JS MAX PRECISE VALUES
BSON.JS_INT_MAX = 0x20000000000000;  // Any integer up to 2^53 can be precisely represented by a double.
BSON.JS_INT_MIN = -0x20000000000000;  // Any integer down to -2^53 can be precisely represented by a double.

// Internal long versions
var JS_INT_MAX_LONG = Long.fromNumber(0x20000000000000);  // Any integer up to 2^53 can be precisely represented by a double.
var JS_INT_MIN_LONG = Long.fromNumber(-0x20000000000000);  // Any integer down to -2^53 can be precisely represented by a double.

module.exports = calculateObjectSize;
