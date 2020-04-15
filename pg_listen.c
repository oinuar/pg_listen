#include <errno.h>
#include <libpq-fe.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define BUFSZ 512

static PGconn *connection = NULL;
static char *channel = NULL;

void clean_and_exit(int exit_code)
{
   if (channel != NULL)
      PQfreemem(channel);

   if (connection != NULL)
      PQfinish(connection);

   wait(NULL); /* Wait for all forked childs. */
   exit(exit_code);
}

int print_log(const char *sev, const char *fmt, ...)
{
   va_list ap;
   time_t now = time(NULL);
   char timestamp[128];
   int res;

   strftime(timestamp, sizeof timestamp, "%Y-%m-%dT%H:%M:%S", gmtime(&now));
   res = fprintf(stderr, "%s - pg_listen - %s - ", timestamp, sev);

   va_start(ap, fmt);
   res += vfprintf(stderr, fmt, ap);
   va_end(ap);

   return res + fprintf(stderr, "\n");
}

void sig_handler(int signum, siginfo_t *info, void *context)
{
   (void)info;
   (void)context;

   if (signum == SIGTERM || signum == SIGINT)
   {
      print_log("INFO", "Received exit signal.");
      clean_and_exit(EXIT_SUCCESS);
   }
}

int exec_pipe(const char *cmd, char **cmd_argv, const char *input)
{
   int pipefds[2];

   /* we'll send "input" through pipe to stdin */
   if (errno = 0, pipe(pipefds) < 0)
   {
      print_log("ERROR", "pipe2(): %s", strerror(errno));
      return 0;
   }

   switch (errno = 0, fork())
   {
   case -1:
      print_log("ERROR", "fork(): %s", strerror(errno));
      close(pipefds[0]);
      close(pipefds[1]);
      return 0;

   case 0: /* Child - reads from pipe */
      /* Write end is unused */
      close(pipefds[1]);
      /* read from pipe as stdin */
      if (errno = 0, dup2(pipefds[0], STDIN_FILENO) < 0)
      {
         print_log("ERROR",
                   "Unable to assign stdin to pipe: %s",
                   strerror(errno));
         close(pipefds[0]);
         exit(EXIT_FAILURE);
      }

      if (errno = 0, execv(cmd, cmd_argv) < 0)
      {
         print_log("ERROR", "execv(%s): %s",
                   cmd, strerror(errno));
         close(pipefds[0]);
         clean_and_exit(EXIT_FAILURE);
      }
      /* should not get here */
      break;

   default:              /* Parent - writes to pipe */
      close(pipefds[0]); /* Read end is unused */
      write(pipefds[1], input, strlen(input));
      close(pipefds[1]);
      break;
   }

   return 1;
}

int reset_if_necessary(PGconn *conn)
{
   unsigned int seconds = 0;

   if (PQstatus(conn) == CONNECTION_OK)
      return 0;

   do
   {
      if (seconds == 0)
         seconds = 1;
      else
      {
         print_log("ERROR", "Connection failed.\nSleeping %u seconds.", seconds);
         sleep(seconds);
         seconds *= 2;
      }

      print_log("INFO", "Reconnecting to database...");
      PQreset(conn);
   } while (PQstatus(conn) != CONNECTION_OK);

   return 1;
}

void begin_listen(PGconn *conn, const char *chan)
{
   PGresult *res;
   char sql[7 + BUFSZ + 1];

   print_log("INFO", "Listening on channel %s", chan);

   snprintf(sql, 7 + BUFSZ + 1, "LISTEN %s", chan);
   res = PQexec(conn, sql);

   if (PQresultStatus(res) != PGRES_COMMAND_OK)
   {
      print_log("CRITICAL", "LISTEN command failed: %s", PQerrorMessage(conn));
      PQclear(res);
      clean_and_exit(EXIT_FAILURE);
   }

   PQclear(res);
}

void listen_forever(PGconn *conn, const char *chan, const char *cmd, char **cmd_argv)
{
   int sock;
   PGnotify *notify;
   struct pollfd pfd[1];

   begin_listen(conn, chan);

   for (;;)
   {
      if (reset_if_necessary(conn))
         begin_listen(conn, chan);

      sock = PQsocket(conn);
      if (sock < 0)
      {
         print_log("CRITICAL",
                   "Failed to get libpq socket: %s\n",
                   PQerrorMessage(conn));
         clean_and_exit(EXIT_FAILURE);
      }

      pfd[0].fd = sock;
      pfd[0].events = POLLIN;
      if (errno = 0, poll(pfd, 1, -1) < 0)
      {
         print_log("CRITICAL", "poll(): %s", strerror(errno));
         clean_and_exit(EXIT_FAILURE);
      }

      PQconsumeInput(conn);

      while ((notify = PQnotifies(conn)) != NULL)
      {
         if (!cmd) {
            fputs(notify->extra, stdout);
            fflush(stdout);
         }

         else if (!exec_pipe(cmd, cmd_argv, notify->extra))
         {
            PQfreemem(notify);
            clean_and_exit(EXIT_FAILURE);
         }

         PQfreemem(notify);
      }
   }
}

int main(int argc, char **argv)
{
   struct sigaction sigact;

   if (argc < 3)
   {
      fprintf(stderr,
              "USAGE: %s postgresql://user:password@db-url:port channel [/path/to/program] [args]\n",
              argv[0]);

      return EXIT_FAILURE;
   }

   memset(&sigact, 0, sizeof(sigact));
   sigact.sa_sigaction = sig_handler;

   if (sigaction(SIGTERM, &sigact, NULL) != sigaction(SIGINT, &sigact, NULL) != 0)
   {
      print_log("CRITICAL", "Cannot register SIGTERM and SIGINT handlers.");
      return EXIT_FAILURE;
   }

   connection = PQconnectdb(argv[1]);

   if (PQstatus(connection) != CONNECTION_OK)
   {
      print_log("CRITICAL", PQerrorMessage(connection));
      clean_and_exit(EXIT_FAILURE);
   }

   channel = PQescapeIdentifier(connection, argv[2], BUFSZ);

   if (channel == NULL)
   {
      print_log("CRITICAL", PQerrorMessage(connection));
      clean_and_exit(EXIT_FAILURE);
   }

   /* safe since argv[argc] == NULL by C99 5.1.2.2.1 */
   listen_forever(connection, channel, argv[3], argv + 3);

   return EXIT_SUCCESS;
}
