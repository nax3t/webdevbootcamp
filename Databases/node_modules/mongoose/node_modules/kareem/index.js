'use strict';

function Kareem() {
  this._pres = {};
  this._posts = {};
}

Kareem.prototype.execPre = function(name, context, callback) {
  var pres = this._pres[name] || [];
  var numPres = pres.length;
  var numAsyncPres = pres.numAsync || 0;
  var currentPre = 0;
  var asyncPresLeft = numAsyncPres;
  var done = false;

  if (!numPres) {
    return process.nextTick(function() {
      callback(null);
    });
  }

  var next = function() {
    if (currentPre >= numPres) {
      return;
    }
    var pre = pres[currentPre];

    if (pre.isAsync) {
      pre.fn.call(
        context,
        function(error) {
          if (error) {
            if (done) {
              return;
            }
            done = true;
            return callback(error);
          }

          ++currentPre;
          next.apply(context, arguments);
        },
        function(error) {
          if (error) {
            if (done) {
              return;
            }
            done = true;
            return callback(error);
          }

          if (--numAsyncPres === 0) {
            return callback(null);
          }
        });
    } else if (pre.fn.length > 0) {
      var args = [function(error) {
        if (error) {
          if (done) {
            return;
          }
          done = true;
          return callback(error);
        }

        if (++currentPre >= numPres) {
          if (asyncPresLeft > 0) {
            // Leave parallel hooks to run
            return;
          } else {
            return callback(null);
          }
        }

        next.apply(context, arguments);
      }];
      if (arguments.length >= 2) {
        for (var i = 1; i < arguments.length; ++i) {
          args.push(arguments[i]);
        }
      }
      pre.fn.apply(context, args);
    } else {
      pre.fn.call(context);
      if (++currentPre >= numPres) {
        if (asyncPresLeft > 0) {
          // Leave parallel hooks to run
          return;
        } else {
          return process.nextTick(function() {
            callback(null);
          });
        }
      }
      next();
    }
  };

  next();
};

Kareem.prototype.execPost = function(name, context, args, callback) {
  var posts = this._posts[name] || [];
  var numPosts = posts.length;
  var currentPost = 0;

  if (!numPosts) {
    return process.nextTick(function() {
      callback.apply(null, [null].concat(args));
    });
  }

  var next = function() {
    var post = posts[currentPost];

    if (post.length > args.length) {
      post.apply(context, args.concat(function(error) {
        if (error) {
          return callback(error);
        }

        if (++currentPost >= numPosts) {
          return callback.apply(null, [null].concat(args));
        }

        next();
      }));
    } else {
      post.apply(context, args);

      if (++currentPost >= numPosts) {
        return callback.apply(null, [null].concat(args));
      }

      next();
    }
  };

  next();
};

Kareem.prototype.wrap = function(name, fn, context, args, useLegacyPost) {
  var lastArg = (args.length > 0 ? args[args.length - 1] : null);
  var _this = this;

  this.execPre(name, context, function(error) {
    if (error) {
      if (typeof lastArg === 'function') {
        return lastArg(error);
      }
      return;
    }

    var end = (typeof lastArg === 'function' ? args.length - 1 : args.length);

    fn.apply(context, args.slice(0, end).concat(function() {
      if (arguments[0]) {
        // Assume error
        return typeof lastArg === 'function' ?
          lastArg(arguments[0]) :
          undefined;
      }

      if (useLegacyPost && typeof lastArg === 'function') {
        lastArg.apply(context, arguments);
      }

      var argsWithoutError = Array.prototype.slice.call(arguments, 1);
      _this.execPost(name, context, argsWithoutError, function() {
        if (arguments[0]) {
          return typeof lastArg === 'function' ?
            lastArg(arguments[0]) :
            undefined;
        }

        return typeof lastArg === 'function' && !useLegacyPost ?
          lastArg.apply(context, arguments) :
          undefined;
      });
    }));
  });
};

Kareem.prototype.createWrapper = function(name, fn, context) {
  var _this = this;
  return function() {
    var args = Array.prototype.slice.call(arguments);
    _this.wrap(name, fn, context, args);
  };
};

Kareem.prototype.pre = function(name, isAsync, fn, error) {
  if (typeof arguments[1] !== 'boolean') {
    error = fn;
    fn = isAsync;
    isAsync = false;
  }

  this._pres[name] = this._pres[name] || [];
  var pres = this._pres[name];

  if (isAsync) {
    pres.numAsync = pres.numAsync || 0;
    ++pres.numAsync;
  }

  pres.push({ fn: fn, isAsync: isAsync });

  return this;
};

Kareem.prototype.post = function(name, fn) {
  (this._posts[name] = this._posts[name] || []).push(fn);
  return this;
};

Kareem.prototype.clone = function() {
  var n = new Kareem();
  for (var key in this._pres) {
    n._pres[key] = this._pres[key].slice();
  }
  for (var key in this._posts) {
    n._posts[key] = this._posts[key].slice();
  }

  return n;
};

module.exports = Kareem;
