var tap = require('tap')

var catMe = require('./')

tap.test('returns a cat at random', function (t) {
  t.plan(1)
  var cat = catMe()
  t.equal(typeof cat, 'string', 'does it')
  console.log(cat)
})

var tubs = '  /\\ ___ /\\' + '\n' +
' (  o   o  )' + '\n' +
'  \\  >#<  /' + '\n' +
'  /       \\' + '\n' +
' /         \\       ^' + '\n' +
'|           |     //' + '\n' +
' \\         /    //' + '\n' +
'  ///  ///   --'

tap.test('returns a specific cat', function (t) {
  t.plan(1)
  var cat = catMe('tubby')
  t.equal(cat, tubs, 'does it')
  console.log(cat)
})

tap.test('returns a list of cat names', function (t) {
  t.plan(1)
  var catNames = catMe.catNames
  t.ok(catNames.indexOf('grumpy') !== -1, 'does it')
})
