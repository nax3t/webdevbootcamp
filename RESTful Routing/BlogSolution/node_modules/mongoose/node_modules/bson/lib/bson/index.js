try {
  exports.BSONPure = require('./bson');
  exports.BSONNative = require('./bson');
} catch(err) {
}

[ './binary_parser'
  , './binary'
  , './code'
  , './map'
  , './db_ref'
  , './double'
  , './max_key'
  , './min_key'
  , './objectid'
  , './regexp'
  , './symbol'
  , './timestamp'
  , './long'].forEach(function (path) {
  	var module = require('./' + path);
  	for (var i in module) {
  		exports[i] = module[i];
    }
});

// Exports all the classes for the PURE JS BSON Parser
exports.pure = function() {
  var classes = {};
  // Map all the classes
  [ './binary_parser'
    , './binary'
    , './code'
    , './map'
    , './db_ref'
    , './double'
    , './max_key'
    , './min_key'
    , './objectid'
    , './regexp'
    , './symbol'
    , './timestamp'
    , './long'
    , '././bson'].forEach(function (path) {
    	var module = require('./' + path);
    	for (var i in module) {
    		classes[i] = module[i];
      }
  });
  // Return classes list
  return classes;
}

// Exports all the classes for the NATIVE JS BSON Parser
exports.native = function() {
  var classes = {};
  // Map all the classes
  [ './binary_parser'
    , './binary'
    , './code'
    , './map'
    , './db_ref'
    , './double'
    , './max_key'
    , './min_key'
    , './objectid'
    , './regexp'
    , './symbol'
    , './timestamp'
    , './long'
  ].forEach(function (path) {
      var module = require('./' + path);
      for (var i in module) {
        classes[i] = module[i];
      }
  });

  // Catch error and return no classes found
  try {
    classes['BSON'] = require('./bson');
  } catch(err) {
    return exports.pure();
  }

  // Return classes list
  return classes;
}
