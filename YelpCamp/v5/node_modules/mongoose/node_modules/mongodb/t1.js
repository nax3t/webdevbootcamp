var MongoClient = require('./').MongoClient;

MongoClient.connect('mongodb://localhost:31000/test?rs_name=rs', function (err, db) {
  var collection = db.collection('test');

  setInterval(function() {
    console.dir(db.serverConfig.s.replset.s.replState.arbiters[0].connections().length)
  }, 1000);

  // db.close();
  // collection.insertMany([{a:1}, {a:2}], {w:1}, function (err, docs) {
  //   if (err) {
  //     console.log("ERROR");
  //   }

  //   collection.find().sort({'a': -1}).toArray(function(err, items) {
  //     if (err) {
  //       console.log("ERROR");
  //     }
  //     console.log("Items: ", items);
  //   });
  // });
});

// var MongoClient = require('./').MongoClient;

// MongoClient.connect('mongodb://localhost:27017/page-testing', function (err, db) {
//   collection = db.collection('test');

//   collection.insertMany([{a:1}, {a:2}], {w:1}, function (err, docs) {
//     if (err) {
//       console.log("ERROR");
//     }

//     collection.find().sort({'a': -1}).toArray(function(err, items) {
//       if (err) {
//         console.log("ERROR");
//       }
//       console.log("Items: ", items);
//     });
//   });
// });
// var database = null;
//
// var MongoClient = require('./').MongoClient;
//
// function connect_to_mongo(callback) {
//   if (database != null) {
//     callback(null, database);
//   } else {
//     var connection = "mongodb://127.0.0.1:27017/test_db";
//     MongoClient.connect(connection, {
//       server : {
//         reconnectTries : 5,
//         reconnectInterval: 1000,
//         autoReconnect : true
//       }
//     }, function (err, db) {
//       database = db;
//       callback(err, db);
//     });
//   }
// }
//
// function log(message) {
//   console.log(new Date(), message);
// }
//
// var queryNumber = 0;
//
// function make_query(db) {
//   var currentNumber = queryNumber;
//   ++queryNumber;
//   log("query " + currentNumber + ": started");
//
//   setTimeout(function() {
//       make_query(db);
//   }, 5000);
//
//   var collection = db.collection('test_collection');
//   collection.findOne({},
//     function (err, result) {
//       if (err != null) {
//         log("query " + currentNumber + ": find one error: " + err.message);
//         return;
//       }
//       log("query " + currentNumber + ": find one result: " + result);
//     }
//   );
// }
//
// connect_to_mongo(
//   function(err, db) {
//     if (err != null) {
//       log(err.message);
//       return;
//     }
//
//     make_query(db);
//   }
// );
