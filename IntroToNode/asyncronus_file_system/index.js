const fs = require("fs");
//making directory
// fs.mkdir("bio", (err) => {
//   console.log("folder is created");
// });
// //creating the file and writing data in it
// fs.writeFile(
//   "bio/Data.txt",
//   "Name:Divyansh kr singh \n DOB:09/11/2001",
//   (err) => {
//     console.log("file is created");
//   }
// );
// //adding extra data in file
// fs.appendFile("bio/Data.txt", "\nRoll No. : 057", (err) => {
//   console.log("data is appended");
// });
//displaying info of the file
let inside_data = fs.readFile("bio/Data.txt", "utf-8", (err, inside_data) => {
  console.log(inside_data);
});

// //deleting the folder
// fs.rmdir("bio", (err) => {
//   console.log("folder deleted");
// });
