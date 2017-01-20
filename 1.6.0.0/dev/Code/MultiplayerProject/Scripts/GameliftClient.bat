REM This batch file runs the MultiplayerProjectLauncher with the specified cvars (console variables) 
REM and then loads the GameLiftLobby level.
REM
REM CVars
REM sv_port - Network port for your game (33435 for the sample game)
REM gamelift_fleet_id - The fleet ID for your active Amazon GameLift fleet
REM gamelift_aws_access_key - Your AWS access key
REM gamelift_aws_secret_key - Your AWS secret key
REM 
REM Instructions
REM Uncomment the last line, update the values for each cvar and copy to your dev/Bin64 folder.

REM MultiplayerProjectLauncher.exe  +sv_port <network port> +gamelift_fleet_id <fleet id> +gamelift_aws_access_key <AWS access key> +gamelift_aws_secret_key <AWS secret key> +map gameliftlobby
