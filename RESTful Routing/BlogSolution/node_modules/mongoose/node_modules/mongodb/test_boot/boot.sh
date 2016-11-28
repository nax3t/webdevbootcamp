mongod --verbose --smallfiles --noprealloc --setParameter enableTestCommands=1 \
--port 39000 --dbpath ./data --sslMode allowSSL --sslCAFile ./ca.pem \
--sslPEMKeyFile ./server_password.pem --sslPEMKeyPassword serverpassword --nojournal