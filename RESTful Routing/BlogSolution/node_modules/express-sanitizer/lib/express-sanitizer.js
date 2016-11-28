var sanitizer = require('sanitizer');
    
var expressSanitizer = function(options) {
  options = options || {};

  /*
    //options placeholder
    var _options = {};

    _options.errorFormatter = options.errorFormatter || function(param, msg, value) {
      return {
        param : param,
        msg   : msg,
        value : value
      };
    };
  */

  return function(req, res, next) {
    req.sanitize = function(param) {
      if (param) {
        return sanitizer.sanitize(param);
      }
    };
    next();
  };

};
module.exports = expressSanitizer;
