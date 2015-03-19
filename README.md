Welcome to Unreal Tournament!
=============================

This is the project page for **Unreal Tournament** on GitHub.  Unreal Tournament is being created in close collaboration between Epic and the Unreal Engine 4 developer community.  You'll be able to see live commits from Epic developers along with integrated code submissions from the community!  You can also use this code as a starting point for experimentation and Unreal Tournament mod creation.

As described in the [License Agreement](https://github.com/EpicGames/UnrealTournament/blob/master/LICENSE.pdf), the code and assets residing in this repository may only be used for Unreal Tournament submissions.

We now have a basic version of Unreal Tournament deathmatch and CTF which provide a basis for design iteration and further development. You can follow our progress on this fork, and the code is ready for extending or modding. If you want to get involved, join us on our [forums](http://forums.unrealengine.com/forumdisplay.php?34-Unreal-Tournament) to participate in design discussions, find projects that need help, and discover other opportunities to contribute.

Check out the [Unreal Tournament wiki](https://wiki.unrealengine.com/Unreal_Tournament_Development) for documentation on the design and development process of the game.



Required Third Party Software
---------------------

**Windows:**

Be sure to have [Visual Studio 2013](http://go.microsoft.com/?linkid=9832280) installed.  You can use any desktop version of Visual Studio 2013, including the free version:  [Visual Studio 2013 Express for Windows Desktop](http://go.microsoft.com/?linkid=9832280).

You need [DirectX End-User Runtimes (June 2010)](http://www.microsoft.com/en-us/download/details.aspx?id=8109) to avoid compilation errors.  Most users will already have this, if you ahve ever installed DirectX runtimes to play a game.
 
**Mac OSX:**

Be sure to have [Xcode 5.1](https://itunes.apple.com/us/app/xcode/id497799835) installed.

How to get started (Windows)
-------------------

The current Unreal Tournament project is based on Unreal Engine version 4.7. The engine source is now included in our repository for your convenience. Visual Studio 2013 is required for Windows usage.

**Downloading the Unreal Tournament project:**

- Download the project by clicking the **Download ZIP** button on this page.
- Unzip the files to a folder on your computer.  
- Run the Setup.bat file in the directory you've created. This will download the necessary binary content not stored on GitHub.

**Compiling the Unreal Tournament project:**
- Use Visual Studio to open UnrealSwarm.sln located in Engine\Source\Programs\UnrealSwarm. Select Development mode in the Solution Configuration drop down menu.
- Right click on the Agent project and select Properties. Go to the Signing tab and uncheck "Sign the ClickOnce manifests".
- Build the Agent project by right clicking on it and selecting Build. You may now close the UnrealSwarm solution.
- Run the GenerateProjectFiles.bat file in the directory you unzipped the project to. This will generate UE4.sln.
- Open UE4.sln in Visual Studio. Set the Solution Configuration drop down menu to Development Editor.
- Build the ShaderCompileWorker and UnrealLightmass projects from the Programs folder.
- Build the UnrealTournament project. UE4Editor.exe will then be available in Engine\Binaries\Win64\.
- Run "UE4Editor.exe UnrealTournament" from a command line or right click the Unreal Tournament project in Visual Studio and start a debug session.



Contributing through GitHub
-----------------------

One way to contribute changes is to send a GitHub [Pull Request](https://help.github.com/articles/using-pull-requests).

**To get started using GitHub:**

- You'll want to create your own Unreal Tournament **fork** by clicking the __Fork button__ in the top right of this page.
- Next, [install a Git client](http://help.github.com/articles/set-up-git) on your computer.
- Use the GitHub program to **Sync** the project's files to a folder on your machine.
- You can now open up **UnrealTournament.uproject** in the editor!
- If you're a programmer, follow the platform specific steps below to set up the C++ project. 
- Using the GitHub program, you can easily **submit contributions** back up to your **fork**.  These files will be visible to all subscribers, and you can advertise your changes in the [Unreal Tournament forums](http://forums.unrealengine.com/forumdisplay.php?34-Unreal-Tournament).
- When you're ready to send the changes to the Unreal Tournament team for review, simply create a [Pull Request](https://help.github.com/articles/using-pull-requests).


