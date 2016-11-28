var MongoClient = require('./').MongoClient;

MongoClient.connect('mongodb://localhost:27017/test', function(err, db) {
  var col = db.collection('test');
  col.ensureIndex({dt:-1}, function() {
    var docs = [];
    for(var i = 0; i < 100; i++) {
      docs.push({a:i, dt:i, ot:i});
    }
    console.log("------------------------------- 0")

    col.insertMany(docs, function() {
      // Start firing finds

      for(var i = 0; i < 100; i++) {
        setInterval(function() {
          col.find({}, {_id: 0, ot:0}).limit(2).sort({dt:-1}).toArray(function(err) {
            console.log("-------------------------------- 1")
          });
        }, 10)
      }

      // while(true) {
      //
      //   // console.log("------------------------------- 1")
      //   col.find({}, {_id: 0, ot:0}).limit(2).sort({dt:-1}).toArray(function(err) {
      //     console.log("-------------------------------- 1")
      //   });
      // }
    });
  });
});
