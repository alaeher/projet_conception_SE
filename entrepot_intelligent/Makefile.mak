all:
	gcc serveur/gestionnaire_outils.c -lws2_32 -o serveur.exe
	gcc client/bras_robotique.c -lws2_32 -o robot.exe

run_server:
	.\serveur.exe

run_client:
	.\robot.exe