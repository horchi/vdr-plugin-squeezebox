/*
 * player.c: A plugin for the Video Disk Recorder
 *
 * See the README file for copyright information and how to reach the author.
 *
 */

#include <sys/wait.h>

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include "lib/common.h"

#include "config.h"
#include "squeezebox.h"

//***************************************************************************
// Squeeze Player
//***************************************************************************

cSqueezePlayer::cSqueezePlayer() 
   : cPlayer(pmAudioOnlyBlack)    // pmExtern_THIS_SHOULD_BE_AVOIDED) // pmAudioOnly)
{
   running = no;
   pid = 0;
}

cSqueezePlayer::~cSqueezePlayer()
{
   Stop();
}

void cSqueezePlayer::Activate(bool on)
{
   if (on) 
      Start();
   else 
      Stop();
}

void cSqueezePlayer::Stop()
{   
   stopPlayer();
   Cancel(3);             // wait up to 3 seconds for thread was stopping
}

void cSqueezePlayer::Action()
{
   startPlayer();
}

//***************************************************************************
// Start Player
//***************************************************************************

int cSqueezePlayer::startPlayer()
{
   int status;
   FILE* out;
   char buf[1024] = "";
   int fd[2];
   int wrfd[2];
   int res = 0;

   if (pid > 0)
   {
      stopPlayer();
      sleep(2);   // sometimes pulseaudio need some seconds
   }

   res += pipe(fd);
   res += pipe(wrfd);

   if (res != 0)
   {
      tell(eloAlways, "Creating pipe failed, %s\n", strerror(errno));
      return -1;
   }

   if ((pid = fork()) < 0)
   {
      tell(eloAlways, "fork failed with %s\n", strerror(errno));
      return -1;
   }
   
   if (pid == 0)     // child code
   {
      char* argv[30];
      int argc = 0;

      dup2(fd[1], STDERR_FILENO);   // Redirect stderr into writing end of pipe
      dup2(fd[1], STDOUT_FILENO);   // Redirect stdout into writing end of pipe
      dup2(wrfd[0], STDIN_FILENO);  // Redirect reading end of pipe into stdin
     
      // Now that we have copies where needed, we can close all the child's other references
      // to the pipes.

      close(fd[0]);
      close(fd[1]);
      close(wrfd[0]);
      close(wrfd[1]);
      
      // create argument array

      argv[argc++] = strdup(cfg.squeezeCmd);
      argv[argc++] = strdup("-s");
      argv[argc++] = strdup(cfg.lmcHost);
      argv[argc++] = strdup("-m"); 
      argv[argc++] = strdup(cfg.mac);
      argv[argc++] = strdup("-n");
      argv[argc++] = strdup(cfg.playerName);

      if (!isEmpty(cfg.audioDevice))
      {
         argv[argc++] = strdup("-o");
         argv[argc++] = strdup(cfg.audioDevice);
      }

      if (!isEmpty(cfg.alsaOptions))
      {
         argv[argc++] = strdup("-a");
         argv[argc++] = strdup(cfg.alsaOptions);
      }
      
      argv[argc] = 0;
      
      // start player ..
      
      std::string tmp;

      for (int i = 0; i < argc; i++)
         tmp += " " + std::string(argv[i]);

      tell(eloAlways, "Starting player with '%s", tmp.c_str());
 
      execv(cfg.squeezeCmd, argv);

      tell(eloAlways, "Process squeezelite ended unexpectedly, reason was '%s'\n", strerror(errno));

      // cleanup ..

      for (int i = 0; i < argc; i++)
         free(argv[i]);

      return -1;
   }
   
   // parent code

   close(fd[1]);     // Don't need writing end of the stderr pipe in parent.
   close(wrfd[0]);   // Don't need the reading end of the stdin pipe in the parent
   
   // take a breath to give squeezelite time to initalize

   usleep(500000);
   tell(eloAlways, "started %s with pid %d\n", cfg.squeezeCmd, pid);
   running = yes;

   out = fdopen(fd[0], "r");
      
   // Wait for the child to quit 
   
   while (waitpid(pid, &status, WNOHANG) == 0)
   {
      while (fgets(buf, sizeof(buf), out))
      {
         buf[strlen(buf)-1] = 0;
         tell(eloAlways, "[squeezelite] %s\n", buf);
      }
      
      usleep(10000);
   }

   close(fd[0]);     // Close the reading end of the stderr pipe
   close(wrfd[1]);   // Close the writing end of the stdout pipe

   pid = 0;
   tell(eloAlways, "%s exited with %d\n", cfg.squeezeCmd, status);

   return 0;
}

//***************************************************************************
// Stop Player
//***************************************************************************

int cSqueezePlayer::stopPlayer()
{
   running = no;

   if (pid)
   {
      tell(eloAlways, "stopping player");

      if (kill(pid, SIGINT) != 0)
      {
         sleep(1);
         kill(pid, SIGKILL);
      }

      if (kill(pid, 0) != 0)
         tell(eloAlways, "Stopping process '%s' failed, error was '%s'", cfg.squeezeCmd, strerror(errno));
      else 
         pid = 0;
   }

   return pid == 0 ? 0 : -1;
}
