var MongoClient = require('./').MongoClient
    , assert = require('assert')
    , cappedCollectionName = "capped_test";
    

function capitalizeFirstLetter(string) {
    return string.charAt(0).toUpperCase() + string.slice(1);
}
  
 function createTailedCursor(db, callback) {
  var collection = db.collection(cappedCollectionName)
      , cursor = collection.find({}, { tailable: true, awaitdata: true, numberOfRetries: Number.MAX_VALUE})
      , stream = cursor.stream()
      , statusGetters = ['notified', 'closed', 'dead', 'killed'];

  console.log('After stream open');
  statusGetters.forEach(function (s) {
    var getter = 'is' + capitalizeFirstLetter(s);
    console.log("cursor " + getter + " => ", cursor[getter]());
  });

  
  stream.on('error', callback);
  stream.on('end', callback.bind(null, 'end'));
  stream.on('close', callback.bind(null, 'close'));
  stream.on('readable', callback.bind(null, 'readable'));
  stream.on('data', callback.bind(null, null, 'data'));
  
  console.log('After stream attach events');
  statusGetters.forEach(function (s) {
    var getter = 'is' + capitalizeFirstLetter(s);
    console.log("cursor " + getter + " => ", cursor[getter]());
  });
 }
 
 function cappedStreamEvent(err, evName, data) {
   if (err) {
     console.log("capped stream got error", err);
     return;
   }
   
   if (evName) {
     console.log("capped stream got event", evName);
   }
   
   if (data) {
     console.log("capped stream got data", data);
   }   
 }
 
 
// Connection URL
var url = 'mongodb://localhost:27017/myproject';
// Use connect method to connect to the Server
MongoClient.connect(url, function(err, db) {
  assert.equal(null, err);
  console.log("Connected correctly to server");
  
  db.createCollection(cappedCollectionName,
                                { "capped": true,
                                  "size": 100000,
                                  "max": 5000 },
                                function(err, collection) { 
                
      assert.equal(null, err);              
      console.log("Created capped collection " + cappedCollectionName);
      
      createTailedCursor(db, cappedStreamEvent);
  });
  
  
  //db.close();
});