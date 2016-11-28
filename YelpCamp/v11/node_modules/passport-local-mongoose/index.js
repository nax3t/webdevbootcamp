var crypto = require('crypto');
var LocalStrategy = require('passport-local').Strategy;
var errors = require('./lib/errors.js');
var scmp = require('scmp');

module.exports = function(schema, options) {
  options = options || {};
  options.saltlen = options.saltlen || 32;
  options.iterations = options.iterations || 25000;
  options.keylen = options.keylen || 512;
  options.encoding = options.encoding || 'hex';
  options.digestAlgorithm = options.digestAlgorithm || 'sha256'; // To get a list of supported hashes use crypto.getHashes()
  options.passwordValidator = options.passwordValidator || function(password, cb) { cb(null); };

  // Populate field names with defaults if not set
  options.usernameField = options.usernameField || 'username';
  options.usernameUnique = options.usernameUnique === undefined ? true : options.usernameUnique;

  // Populate username query fields with defaults if not set,
  // otherwise add username field to query fields.
  if (options.usernameQueryFields) {
    options.usernameQueryFields.push(options.usernameField);
  } else {
    options.usernameQueryFields = [options.usernameField];
  }

  // option to convert username to lowercase when finding
  options.usernameLowerCase = options.usernameLowerCase || false;

  options.hashField = options.hashField || 'hash';
  options.saltField = options.saltField || 'salt';

  if (options.limitAttempts) {
    options.lastLoginField = options.lastLoginField || 'last';
    options.attemptsField = options.attemptsField || 'attempts';
    options.interval = options.interval || 100; // 100 ms
    options.maxInterval = options.maxInterval || 300000; // 5 min
    options.maxAttempts = options.maxAttempts || Infinity;
  }

  options.errorMessages = options.errorMessages || {};
  options.errorMessages.MissingPasswordError = options.errorMessages.MissingPasswordError || 'No password was given';
  options.errorMessages.AttemptTooSoonError = options.errorMessages.AttemptTooSoonError || 'Account is currently locked. Try again later';
  options.errorMessages.TooManyAttemptsError = options.errorMessages.TooManyAttemptsError || 'Account locked due to too many failed login attempts';
  options.errorMessages.NoSaltValueStoredError = options.errorMessages.NoSaltValueStoredError || 'Authentication not possible. No salt value stored';
  options.errorMessages.IncorrectPasswordError = options.errorMessages.IncorrectPasswordError || 'Password or username are incorrect';
  options.errorMessages.IncorrectUsernameError = options.errorMessages.IncorrectUsernameError || 'Password or username are incorrect';
  options.errorMessages.MissingUsernameError = options.errorMessages.MissingUsernameError|| 'No username was given';
  options.errorMessages.UserExistsError = options.errorMessages.UserExistsError|| 'A user with the given username is already registered';

  var pbkdf2 = function(password, salt, callback) {
    if (crypto.pbkdf2.length >= 6) {
      crypto.pbkdf2(password, salt, options.iterations, options.keylen, options.digestAlgorithm, callback);
    } else {
      crypto.pbkdf2(password, salt, options.iterations, options.keylen, callback);
    }
  };

  var schemaFields = {};

  if (!schema.path(options.usernameField)) {
    schemaFields[options.usernameField] = {type: String, unique: options.usernameUnique};
  }
  schemaFields[options.hashField] = {type: String, select: false};
  schemaFields[options.saltField] = {type: String, select: false};

  if (options.limitAttempts) {
    schemaFields[options.attemptsField] = {type: Number, default: 0};
    schemaFields[options.lastLoginField] = {type: Date, default: Date.now};
  }

  schema.add(schemaFields);

  schema.pre('save', function(next) {
    // if specified, convert the username to lowercase
    if (options.usernameLowerCase && this[options.usernameField]) {
      this[options.usernameField] = this[options.usernameField].toLowerCase();
    }

    next();
  });

  schema.methods.setPassword = function(password, cb) {
    if (!password) {
      return cb(new errors.MissingPasswordError(options.errorMessages.MissingPasswordError));
    }

    var self = this;

    options.passwordValidator(password, function(err) {
      if (err) {
        return cb(err);
      }

      crypto.randomBytes(options.saltlen, function(randomBytesErr, buf) {
        if (randomBytesErr) {
          return cb(randomBytesErr);
        }

        var salt = buf.toString(options.encoding);

        pbkdf2(password, salt, function(pbkdf2Err, hashRaw) {
          if (pbkdf2Err) {
            return cb(pbkdf2Err);
          }

          self.set(options.hashField, new Buffer(hashRaw, 'binary').toString(options.encoding));
          self.set(options.saltField, salt);

          cb(null, self);
        });
      });
    });
  };

  function authenticate(user, password, cb) {
    if (options.limitAttempts) {
      var attemptsInterval = Math.pow(options.interval, Math.log(user.get(options.attemptsField) + 1));
      var calculatedInterval = (attemptsInterval < options.maxInterval) ? attemptsInterval : options.maxInterval;

      if (Date.now() - user.get(options.lastLoginField) < calculatedInterval) {
        user.set(options.lastLoginField, Date.now());
        user.save();
        return cb(null, false, new errors.AttemptTooSoonError(options.errorMessages.AttemptTooSoonError));
      }

      if (user.get(options.attemptsField) >= options.maxAttempts) {
        return cb(null, false, new errors.TooManyAttemptsError(options.errorMessages.TooManyAttemptsError));
      }
    }

    if (!user.get(options.saltField)) {
      return cb(null, false, new errors.NoSaltValueStoredError(options.errorMessages.NoSaltValueStoredError));
    }

    pbkdf2(password, user.get(options.saltField), function(err, hashRaw) {
      if (err) {
        return cb(err);
      }

      var hash = new Buffer(hashRaw, 'binary').toString(options.encoding);

      if (scmp(hash, user.get(options.hashField))) {
        if (options.limitAttempts) {
          user.set(options.lastLoginField, Date.now());
          user.set(options.attemptsField, 0);
          user.save();
        }
        return cb(null, user);
      } else {
        if (options.limitAttempts) {
          user.set(options.lastLoginField, Date.now());
          user.set(options.attemptsField, user.get(options.attemptsField) + 1);
          user.save(function(saveErr) {
            if (saveErr) { return cb(saveErr); }
            if (user.get(options.attemptsField) >= options.maxAttempts) {
              return cb(null, false, new errors.TooManyAttemptsError(options.errorMessages.TooManyAttemptsError));
            } else {
              return cb(null, false, new errors.IncorrectPasswordError(options.errorMessages.IncorrectPasswordError));
            }
          });
        } else {
          return cb(null, false, new errors.IncorrectPasswordError(options.errorMessages.IncorrectPasswordError));
        }
      }
    });

  }

  schema.methods.authenticate = function(password, cb) {
    var self = this;

    // With hash/salt marked as "select: false" - load model including the salt/hash fields form db and authenticate
    if (!self.get(options.saltField)) {
      self.constructor.findByUsername(self.get(options.usernameField), true, function(err, user) {
        if (err) { return cb(err); }

        if (user) {
          return authenticate(user, password, cb);
        } else {
          return cb(null, false, new errors.IncorrectUsernameError(options.errorMessages.IncorrectUsernameError));
        }
      });
    } else {
      return authenticate(self, password, cb);
    }
  };

  if (options.limitAttempts) {
    schema.methods.resetAttempts = function(cb) {
      this.set(options.attemptsField, 0);
      this.save(cb);
    };
  }

  schema.statics.authenticate = function() {
    var self = this;

    return function(username, password, cb) {
      self.findByUsername(username, true, function(err, user) {
        if (err) { return cb(err); }

        if (user) {
          return user.authenticate(password, cb);
        } else {
          return cb(null, false, new errors.IncorrectUsernameError(options.errorMessages.IncorrectUsernameError));
        }
      });
    };
  };

  schema.statics.serializeUser = function() {
    return function(user, cb) {
      cb(null, user.get(options.usernameField));
    };
  };

  schema.statics.deserializeUser = function() {
    var self = this;

    return function(username, cb) {
      self.findByUsername(username, cb);
    };
  };

  schema.statics.register = function(user, password, cb) {
    // Create an instance of this in case user isn't already an instance
    if (!(user instanceof this)) {
      user = new this(user);
    }

    if (!user.get(options.usernameField)) {
      return cb(new errors.MissingUsernameError(options.errorMessages.MissingUsernameError));
    }

    var self = this;
    self.findByUsername(user.get(options.usernameField), function(err, existingUser) {
      if (err) { return cb(err); }

      if (existingUser) {
        return cb(new errors.UserExistsError(options.errorMessages.UserExistsError));
      }

      user.setPassword(password, function(setPasswordErr, user) {
        if (setPasswordErr) {
          return cb(setPasswordErr);
        }

        user.save(function(saveErr) {
          if (saveErr) {
            return cb(saveErr);
          }

          cb(null, user);
        });
      });
    });
  };

  schema.statics.findByUsername = function(username, selectHashSaltFields, cb) {
    if (typeof cb === 'undefined') {
      cb = selectHashSaltFields;
      selectHashSaltFields = false;
    }

    // if specified, convert the username to lowercase
    if (username !== undefined && options.usernameLowerCase) {
      username = username.toLowerCase();
    }

    // Add each username query field
    var queryOrParameters = [];
    for (var i = 0; i < options.usernameQueryFields.length; i++) {
      var parameter = {};
      parameter[options.usernameQueryFields[i]] = username;
      queryOrParameters.push(parameter);
    }

    var query = this.findOne({$or: queryOrParameters});

    if (selectHashSaltFields) {
      query.select('+' + options.hashField + " +" + options.saltField);
    }

    if (options.selectFields) {
      query.select(options.selectFields);
    }

    if (options.populateFields) {
      query.populate(options.populateFields);
    }

    if (cb) {
      query.exec(cb);
    } else {
      return query;
    }
  };

  schema.statics.createStrategy = function() {
    return new LocalStrategy(options, this.authenticate());
  };
};

module.exports.errors = errors;