var MongoClient = require('./').MongoClient;

var index = 0;

MongoClient.connect('mongodb://localhost:27017/test', function(err, db) {
  setInterval(function() {
    db = db.db("index" + index, {noListener:true});
    var collection = db.collection("index" + index);
    collection.insert({a:index})
  }, 1);
});

// var Server = require('./').Server,
//   Db = require('./').Db,
//   ReadPreference = require('./').ReadPreference;
//
// new Db('test', new Server('localhost', 31001), {readPreference: ReadPreference.SECONDARY}).open(function(err, db) {
//
//   db.collection('test').find().toArray(function(err, docs) {
//     console.dir(err)
//     console.dir(docs)
//     db.close();
//   });
// });
