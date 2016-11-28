## Installation

```
npm install sanitizer
```

## Require

```
var sanitizer = require('sanitizer');
```

## Use

See /test/test-sanitzer.js for full documentation.

```
sanitizer.escape('your dirty string'); // Escapes HTML special characters in attribute values as HTML entities
```

```
var yourParser = sanitizer.makeSaxParser(yourHandler); // Given a SAX-like event handler, produce a function that feeds those events and a parameter to the event handler.
```

```
sanitizer.normalizeRCData('your dirty string'); // Escape entities in RCDATA that can be escaped without changing the meaning.
```

```
sanitizer.sanitize('your dirty string'); // Strips unsafe tags and attributes from html.
```

```
exports.unescapeEntities('your string'); // The plain text of a chunk of HTML CDATA which possibly containing.
```

## Caveats

It's use this at your own risk really - Caja HTML Sanitizer was written by people far cleverer than me. I have just repackaged it to solve a problem I had (sanitization on a Node server). It seems to work, and it passes all its tests in re-packaged form - however I don't fully understand its internals so cannot guarantee its security.


## More information

http://code.google.com/p/google-caja/source/browse/trunk/src/com/google/caja/plugin/html-sanitizer.js
