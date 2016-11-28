var assert = require('assert');
var Kareem = require('../');

describe('execPost', function() {
  var hooks;

  beforeEach(function() {
    hooks = new Kareem();
  });

  it('handles errors', function(done) {
    hooks.post('cook', function(eggs, callback) {
      callback('error!');
    });

    hooks.execPost('cook', null, [4], function(error, eggs) {
      assert.equal('error!', error);
      assert.ok(!eggs);
      done();
    });
  });

  it('multiple posts', function(done) {
    hooks.post('cook', function(eggs, callback) {
      setTimeout(
        function() {
          callback();
        },
        5);
    });

    hooks.post('cook', function(eggs, callback) {
      setTimeout(
        function() {
          callback();
        },
        5);
    });

    hooks.execPost('cook', null, [4], function(error, eggs) {
      assert.ifError(error);
      assert.equal(4, eggs);
      done();
    });
  });
});