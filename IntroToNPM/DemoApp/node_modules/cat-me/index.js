var cats = require('./cats')
var catNames = Object.keys(cats)
module.exports = function (cat) {
  if (!cat) cat = catNames[~~(Math.random() * catNames.length)]
  return cats[cat]
}
module.exports.catNames = catNames
