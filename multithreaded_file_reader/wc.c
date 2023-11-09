#include "wc.h"
#include <time.h>
#include <stdio.h>
#include <sys/wait.h>
#include <limits.h>
#include <stdlib.h>

count_t childCount;

void startCount(int blockSize, int nChildProc,
int processNum, int writePipes[], long fsize, FILE *fp) {
    childCount = word_count(fp, processNum * blockSize, blockSize);
    write(writePipes[processNum], &childCount.linecount, sizeof(childCount.linecount));
    write(writePipes[processNum], &childCount.wordcount, sizeof(childCount.wordcount));
    write(writePipes[processNum], &childCount.charcount, sizeof(childCount.charcount));
    close(writePipes[processNum]);
}

int main(int argc, char **argv) {
  count_t count;
  struct timespec begin, end;
  int nChildProc = 0;
  int pipefd[2];
  int nPipes = 0;
  int pid;
  int exitCondition;
  int totalLines = 0;
  int totalWords = 0;
  int totalChar = 0;
  int buf;
  long fsize;
  FILE *fp;
  FILE *childfp;
  static long blockSize;
  int processNum;


  /* 1st arg: filename */
  if(argc < 2) {
    printf("usage: wc <filname> [# processes] [crash rate]\n");
    return 0;
  }

  /* 2nd (optional) arg: number of child processes */
  if (argc > 2) {
      printf("here");
      nChildProc = atoi(argv[2]);
      if(nChildProc < 1) nChildProc = 1;
      if(nChildProc > 10) nChildProc = 10;
  }
  if (argc == 2) {
      nChildProc = 1;
  }

  /* 3rd (optional) arg: crash rate between 0% and 100%.
     Each child process has that much chance to crash*/
  if(argc > 3) {
    crashRate = atoi(argv[3]);
    if(crashRate < 0) crashRate = 0;
    if(crashRate > 50) crashRate = 50;
    printf("crashRate RATE: %d\n", crashRate);
  }

  printf("# of Child Processes: %d\n", nChildProc);
  printf("crashRate RATE: %d\%\n", crashRate);

  // start to measure time
  clock_gettime(CLOCK_REALTIME, &begin);

  // Open file in read-only mode
  fp = fopen(argv[1], "r");

  if(fp == NULL) {
    printf("File open error: %s\n", argv[1]);
    printf("usage: wc <filname>\n");
    return 0;
  }

  // get a file size
  fseek(fp, 0L, SEEK_END);
  fsize = ftell(fp);
  blockSize = fsize / nChildProc;
  blockSize++;

//remember to free this
  pid_t *pids = calloc(nChildProc, sizeof(pid_t));
  int *readPipes = calloc(nChildProc, sizeof(int));
  int *writePipes = calloc(nChildProc, sizeof(int));
  for (int i = 0; i < nChildProc; i++) {
    pipe(pipefd);
    readPipes[nPipes] = pipefd[0];
    writePipes[nPipes] = pipefd[1];
    nPipes++;
    pid = fork();
    //error
    if (pid < 0) {
      printf("Process error");
    }
    //child process
    else if (pid == 0) {
      close(pipefd[0]);
      childfp = fopen(argv[1], "r");
      startCount(blockSize, nChildProc, i, writePipes, fsize, childfp);
      exit(0);
      break;
    }
    //parent process
    else {
      pids[i] = pid;
      close(pipefd[1]);
    }
  }

  //parent process
  if (pid > 0) {
    for (int i = 0; i < nPipes; i++) {
      waitpid(pids[i], &exitCondition, 0);

      //child process aborted
      if (pid > 0 && WIFSIGNALED(exitCondition)) {
          pipe(pipefd);
          readPipes[i] = pipefd[0];
          writePipes[i] = pipefd[1];
          pid = fork();
          //child process
          if (pid == 0) {
              startCount(blockSize, nChildProc, i, writePipes, fsize, fp);
              exit(0);
          }
          //parent process
          else if (pid > 0) {
              pids[i] = pid;
              i--;
          }
      }
      else {

          //number of lines, then words, then chars
          read(readPipes[i], &childCount.linecount, sizeof(int));
          totalLines += childCount.linecount;
          read(readPipes[i],&childCount.wordcount, sizeof(int));
          totalWords += childCount.wordcount;
          read(readPipes[i],&childCount.charcount, sizeof(int));
          totalChar += childCount.charcount;
      }
    }
    fclose(fp);
  }

  if (pid > 0) {
    clock_gettime(CLOCK_REALTIME, &end);
    long seconds = end.tv_sec - begin.tv_sec;
    long nanoseconds = end.tv_nsec - begin.tv_nsec;
    double elapsed = seconds + nanoseconds*1e-9;
    free(readPipes);
    free(writePipes);
    free(pids);

    printf("\n========= %s =========\n", argv[1]);
    printf("Total Lines : %d \n", totalLines);
    printf("Total Words : %d \n", totalWords);
    printf("Total Characters : %d \n", totalChar);
    printf("======== Took %.3f seconds ========\n", elapsed);

    return(0);
  }
}
