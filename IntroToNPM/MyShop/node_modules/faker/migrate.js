var path = require('path'),
    fs = require('fs'),
    mkdirp = require('mkdirp');
    

// read locals
var faker = require('./')
var locales = faker.locales;

var keys = Object.keys(locales);

console.log(keys)

// for every locale, create a new directory
keys.forEach(function(key, i){
  
  var localeDir = path.normalize(__dirname + '/lib/locales/' + key);
  var localeFile = localeDir + "/index.js";

  mkdirp.sync(localeDir);

  var localeData = require(path.normalize(__dirname + '/lib/locales/' + key));
  
  var props = Object.keys(localeData);
  
  console.log('--------');
  console.log(localeDir);
  
  var localeModuleOutput = "";
  
  var metaHeader = "var " + key + " = {};\nmodule['exports'] = " + key + ";\n";
  localeModuleOutput += metaHeader;
  // localeHeader
  console.log(metaHeader);
  
  
  
  props.forEach(function(p, i){

    var subModuleOutput = '';
    
    if (typeof localeData[p] === 'object') {
      var localeSubmoduleRequire = key + "." + p + ' = require("./' + p + '");\n';
      localeModuleOutput += localeSubmoduleRequire;

          // for every property, make a new module
          var propertyDir = path.normalize(__dirname + '/lib/locales/' + key + '/' + p);
          var propertyFile = path.normalize(__dirname + '/lib/locales/' + key + '/' + p + '/index.js');
          var propertyData = localeData[p];

          console.log(propertyFile);

          mkdirp.sync(propertyDir);

          var metaHeader = "var " + p + " = {};\nmodule['exports'] = " + p + ";\n";
          subModuleOutput += metaHeader;
          console.log(metaHeader);
          for (var v in propertyData) {

            var subFileOutput = '';
            if (typeof propertyData[v] === 'object') {

              // make another subfile...
              var subFile = path.normalize(__dirname + '/lib/locales/' + key + '/' + p + '/' + v + '.js');
              subModuleOutput += p + '.' + v + ' = require("./' + v + '");\n';

              //for (var sub in propertyData[v]) {
                var metaProperty = 'module["exports"] = ' + JSON.stringify(propertyData[v], true, 2) + ";\n";
                subFileOutput += metaProperty;
            //  }

              fs.writeFileSync(subFile, subFileOutput);

              console.log(metaProperty);
            } else {
      //        subFileOutput += 'foo';
            }

            console.log('ignoring', v)
          }


         console.log(propertyFile)
         //    if(i == 3) process.exit()
          fs.writeFileSync(propertyFile, subModuleOutput);
      
    } else {
      // TODO: numbers / booleans / date times...not everything is a string
      var localeSubmoduleValue = key + "." + p + ' = "' + localeData[p] + '";\n';
      localeModuleOutput += localeSubmoduleValue;
    }
    
    console.log(localeSubmoduleRequire);
//    console.log('--------')



  });
  
  fs.writeFileSync(localeFile, localeModuleOutput);
  
  console.log(localeFile);
  
  // process.exit();
  
//  console.log(localeDir)
//  console.log(localeData)
  // mkdirp(localeDir, function(){});
});
