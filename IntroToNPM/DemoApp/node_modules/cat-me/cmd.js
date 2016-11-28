#!/usr/bin/env node

var catMe = require('./')
var argv = require('yargs')
  .alias('h', 'help')
  .help('help')
  .usage('C A T   M E\nprint a cat to your console\n' + catMe())
  .example('catMe')
  .example('catMe nyan')
  .alias('c', 'cats')
  .describe('c', 'get list of cat names')
  .argv

var choice = argv._.join(' ')

var output = argv.c ? 'CAT OPTIONS: ' + catMe.catNames.join(', ') : catMe(choice)

process.stdout.write(output)
process.stdout.write('\n')
