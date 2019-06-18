#H1  How to setup Cloud9 type environment on local (PC/Laptop)
These steps are basically for Windows.
Steps 
###H3 To set Up Vscode 
1. Download Vscode (it is an awesome text editor that comes with integrated terminal, just like cloud9)
	Link: [https://code.visualstudio.com/]
2. Install Git (it is version control that basically controls versions of your code, tracks changes and maintain a timeline of your overall project, a great tool)
	Link: [https://git-scm.com/]
 You need to install git and configure it. For this you will need to create your github account (github is like LinkedIn for your code you can make upload ypu code online, and connect with community, and contribute to the open source world!)
 	YouTube link(s): [https://www.youtube.com/watch?v=J_Clau1bYco&t=198s]
3. Open Vscode style as per themes etc.
4. Set git as your default terminal (if you are using mac then it is not necessary however if you are on windows, you need to set up git as ypu default terminal because commands like touch will not work in command palete or powershell)
	Link: [https://stackoverflow.com/questions/42606837/how-do-i-use-bash-on-windows-from-the-visual-studio-code-integrated-terminal] 
	(follow the second option, it is better)

**NOTE**: There are keyboard shortcuts to open up the terminal, add new terminal etc if some how they are not working you need to change the keybindings. For that, go to "Manage" (a little seeting type option on the leftmost-bottom corner of VScode window) Then go to "Keyboard Shortcuts", in search window, type click on View: toggle integrated terminal click on it and press keys ctrl+shift+ '' (backticks) press enter and you are good to go..

Before you move on with nodejs and mongodb installations, you poke around a bit with vscode try new themes, syntax higlighting etc.
YouTube link: [https://www.youtube.com/watch?v=rH1RTwaAeGc&t=18s]

###H3 Set up Nodejs
1.Intall Nodejs (always go for stable version)
	Link:[https://nodejs.org/en/download/]
    after you install node check for the version by typing in terminal "node --version"
    inside you vscode 
    also check for your npm version 

2. Run nodejs code sample from it documentation guide
	Link: [https://nodejs.org/en/docs/guides/getting-started-guide/] 

 else is all simple what Colt does in his videos you just need to to follow along. In case of app.listen until you are not deploying a app in heroku, You can use and port but  if you are deploying an the yelpcamp keep the port value always equal to 3000 other wise it will give and error
 Link: [https://help.heroku.com/P1AVPANS/why-is-my-node-js-app-crashing-with-an-r10-error]
 just use this 

		const PORT = process.env.PORT || 3000;
		app.listen(PORT, () => {
		    console.log(`Our app is running on port ${ PORT }`);
		    console.log("for local host it runs on http://127.0.0.1:3000/");
		});
###H3 Set Up MongoDB
1. Download MongoDb
	Link: [https://www.mongodb.com/download-center]
	YouTube: [https://www.youtube.com/watch?v=sluiQOXKUmQ] {follow along this video but it is not required to install mongodb compass so untick the install checkbox for mongodb compass}
2. Open your command prompt on windows and type the command "./mongod" you should see the mongo daemon running with last line as 'waiting connection on port 27071' like and in another command prompt window type mongo to run mongo shell and should see it running with this sign '>'
3. Repeat the above step inside your VScode git terminal. if it doesn't work follow these 
  For running mongo daemon command you always need to go into the folder where you installed you mongo db 
	like for me it is /c/'Program Files'/MongoDB/Server/4.0/bin. so you need to cd into it
	then run './mongod'
	if the error still persists try
	Link:[https://stackoverflow.com/questions/30835302/mongod-command-not-found-windows7node-js?rq=1]
  For running 'mongo' command you do need to be in the same folder but this is kind of tedious so try this
 	Link: [https://teamtreehouse.com/community/how-to-setup-mongodb-on-windows-cmd-or-gitbash-with-shortcuts]
 	follow the steps for GitBash one change the version fron 3.4  to 4.0

###H3 Set up Heroku
1. To Dowload heroku create an account just like Colt did and dowanload it 
	Link: [https://devcenter.heroku.com/articles/heroku-cli]
	Don't try to go by npm. 
	Check you installation by typing 'heroku --version' inside your VScode git terminal.
2. I repeat this again please use port  === 3000 other it will give error 
   as mentioned in note above
