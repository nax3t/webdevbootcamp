var faker = require('../');

var items = Object.keys(faker);

items = items.filter(function(i){
  if(['locales', 'definitions', 'locale', 'localeFallback'].indexOf(i) === -1) {
    return i;
  }
});

var schema = {
  "methods": {
    "type": "string",
    "enum": []
  }
};

items.forEach(function(item){
//  schema[item] = {};
  for(var q in faker[item]) {
    console.log(item + '.' + q);
    var fnLine = faker[item][q].toString().split('\n').slice(0,1)[0];
    var prop;
    
    fnLine = fnLine.replace('function (', '');
    fnLine = fnLine.replace(') {', '');
    if (fnLine === "") {
      console.log('no arguments')
      prop = {
      };
    } else {
      console.log(fnLine);
      fnLine = fnLine.split(' ');
      prop = {};
      fnLine.forEach(function(arg){
        prop[arg] = {
          type: "any"
        };
      });
    }
    schema.methods.enum.push(item + '.' + q);
    
//    if  ()
  //  schema
  }
  
});

console.log(schema);