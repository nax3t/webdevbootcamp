#Express Routing Assignment

1. Create a brand new Express app from scratch
2. Create a package.json using `npm init` and add express as a dependency
3. In your main app.js file, add 3 different routes:

Visiting "/" should print "Hi there, welcome to my assignment!"
==============================================================
Visiting "/speak/pig" should print "The pig says 'Oink'"
Visiting "/speak/cow" should print "The cow says 'Moo'"
Visiting "/speak/dog" should print "The dog says 'Woof Woof!'"
==============================================================
Visiting "/repeat/hello/3" should print "hello hello hello"
Visiting "/repeat/hello/5" should print "hello hello hello hello hello"
Visiting "/repeat/blah/2"  should print "blah blah"

If a user visits any other route, print:
"Sorry, page not found...What are you doing with your life?"