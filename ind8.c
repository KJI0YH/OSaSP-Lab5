#define failure(str) {perror(str); exit(-1);}
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>

#define PLG_WORDS_COUNT 7

#define SEM_THREAD_NAME "/sem thread name"
#define SEM_FILE_BIGGER "/sem file bigger"
#define SEM_FILE_SMALLER "/sem file smaller"

//char type for finite automaton
typedef enum _charType { ctUnknown = 0, ctDigit = 1, ctLetter = 2, ctSymbol = 3 } charType;

//transition matrix
int transitions[3][4] = {
	{0, 0, 0, 0},
	{0, 2, 2, 2},
	{0, 2, 2, 2}
};

//file info structure
typedef struct _fileInfo {
	char* name;
	long size;
	sem_t* semFile;
	FILE* fp;
} fileInfo;

//word search structure
typedef struct _wSearch {
	fileInfo smaller;
	fileInfo bigger;
	long start;
	FILE* fres;
} wSearch;

sem_t* semThread;
long threadCount;

//get type of a char
charType getCharType(char c) {
	if (c >= '0' && c <= '9')
		return ctDigit;
	else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		return ctLetter;
	else if (c == '-' || c == '_')
		return ctSymbol;
	return ctUnknown;
}

//get word from a file
char* fgetWord(fileInfo fi, long* pos) {

	//file accsess restriction
	if (sem_wait(fi.semFile) == -1)
		perror("Can not wait a semaphore");

	fseek(fi.fp, *pos, SEEK_SET);
	
	char c;
	while ((c = fgetc(fi.fp)) != EOF && getCharType(c) == ctUnknown) {}
	
	long startWord = ftell(fi.fp)-1;
	
	int state = 1;
	while (state != 0 && (c = fgetc(fi.fp)) != EOF) {
		state = transitions[state][getCharType(c)];
	}
	
	long wordSize = ftell(fi.fp)-startWord-1;
	char* word;
	
	if (wordSize > 0) {
		word = (char*)malloc(sizeof(char) * (wordSize + 1));
		if (word == NULL)
			failure("Can not allocate a memory");
			
		long currentPos = ftell(fi.fp);
		fseek(fi.fp, startWord, SEEK_SET);
		fread(word, sizeof(char), wordSize, fi.fp);
		word[wordSize] = '\0';
	}
	else 
		word = NULL;
		
	(*pos) = ftell(fi.fp);

	if (sem_post(fi.semFile) == -11)
		perror("Can not post a semaphore");

	return word;		
}

//open files and semaphores
void prepareFiles(char* file1, char* file2, fileInfo* bigger, fileInfo* smaller) {
	struct stat st1, st2;
	if (stat(file1, &st1) == -1)
		failure("Can not get stat of a file");
		
	if (stat(file2, &st2) == -1)
		failure("Can not get stat of a file");
	
	bigger->size = st1.st_size;
	bigger->name = file1;
	
	smaller->size = st2.st_size;
	smaller->name = file2;
	
	if (smaller->size > bigger->size) {
		fileInfo temp = (*bigger);
		(*bigger) = (*smaller);
		(*smaller) = temp;
	}
	
	//open files
	smaller->fp = fopen(smaller->name, "rt");
	if (smaller->fp == NULL)
		failure("Can not open a file");
	
	bigger->fp = fopen(bigger->name, "rt");
	if (bigger->fp == NULL)
		failure("Can not open a file");
	
	//open semaphores
	sem_unlink(SEM_FILE_SMALLER);
	smaller->semFile = sem_open(SEM_FILE_SMALLER, O_CREAT, 0777, 1);
	 if (smaller->semFile == SEM_FAILED)
		failure("Error on create a semaphore");
		
	sem_unlink(SEM_FILE_BIGGER);
	bigger->semFile = sem_open(SEM_FILE_BIGGER, O_CREAT, 0777, 1);
	if (bigger->semFile == SEM_FAILED)
		failure("Error on create a semaphore");
}

//get next position of the word in a file
long nextPos(fileInfo fi, long start) {
	long next = start;

	sem_wait(fi.semFile);

	fseek(fi.fp, start, SEEK_SET);
	
	char c;
	while ((c = fgetc(fi.fp)) != EOF && getCharType(c) == ctUnknown) {
		next++;
	}
	
	int state = 1;
	while (state != 0 && (c = fgetc(fi.fp)) != EOF) {
		state = transitions[state][getCharType(c)];
		next++;
	}
	
	sem_post(fi.semFile);
	
	return next;
}

//start search word in thread
void* wordSearch (void* arg) {
	fileInfo smaller = ((wSearch*)arg)->smaller;
	fileInfo bigger = ((wSearch*)arg)->bigger;
	FILE* fres = ((wSearch*)arg)->fres;

	//start position in smaller file
	long sStart = ((wSearch*)arg)->start;
	long sPos = sStart;
	
	char* sWord = fgetWord(smaller, &sPos);
	
	//copy a start search word
	char* startWord = NULL;
	if (sWord != NULL) { 
		startWord = (char*)malloc(sizeof(char) * (strlen(sWord) + 1));
		startWord = strcpy(startWord, sWord);
	}	
	
	long bPos = 0, bRet = 0;
	while (sPos < smaller.size-1 && bPos < bigger.size-1) {
	
		//start position in bigger file
		long bStart = bPos;
		char* bWord = fgetWord(bigger, &bPos);		

		//words matching			
		int count = 0;
		while (bWord != NULL && sWord!= NULL && strcmp(sWord, bWord) == 0) {
			count++;
			free(sWord);
			sWord = fgetWord(smaller, &sPos);
			free(bWord);
			bRet = bPos;
			bWord = fgetWord(bigger, &bPos);
		}
			
		//plagiarism catch
		if (count >= PLG_WORDS_COUNT) {
			printf("\nFILE: %s POS: %ld\nFILE: %s POS: %ld\nWORDS: %d LEN: %ld\nSTART WORD: %s\n", bigger.name, bStart, smaller.name, sStart, count, sPos-sStart, startWord);
			fprintf(fres, "\nFILE: %s POS: %ld\nFILE: %s POS: %ld\nWORDS: %d LEN: %ld\nSTART WORD: %s\n", bigger.name, bStart, smaller.name, sStart, count, sPos-sStart, startWord);
			bPos = bRet;				
		}
			
		//return back to smaller start word
		if (count != 0) {
			sPos = sStart;
			free(sWord);
			sWord = fgetWord(smaller, &sPos);
		}				
		free(bWord);
	}
	free(sWord);

	if (startWord != NULL)
		free(startWord);

	if (sem_post(semThread) == -1)
		perror("Can not post a semaphore");
		
	return NULL;
}

//search plagiarism in files
void plagiarismSearch(fileInfo bigger, fileInfo smaller, FILE* fres) {		
	long sPos = 0;
	while (sPos < smaller.size - 1) {
		if (sem_wait(semThread) == -1) {
            perror("Can not wait a semaphore");
            continue;
        }
            
        //fill the thread argument structure
        wSearch* arg = (wSearch*)malloc(sizeof(wSearch));
        arg->smaller = smaller;
        arg->bigger = bigger;
        arg->start = sPos;
        arg->fres = fres;
            
        //get the next position of a word in smaller file
        sPos = nextPos(smaller, sPos);
            
        //create thread
		pthread_t thread;
        if (pthread_create(&thread, NULL, &wordSearch, (void *)arg)) {
            perror("Can not to start a thread");
            continue;
        }
        
        //detach thread
		if (pthread_detach(thread)) {
            perror("Can not detach a thread");
            continue;
        }
	}

	//waiting for all threads to finish
    for (int i = 0; i < threadCount; ++i) {
        if (sem_wait(semThread) == -1) {
            perror("Can not to wait a semaphore");
        }
    }

	return;
}

void main(int argc, char* argv[]) {

	//check argumenst count
	if (argc < 5) {
		failure("Invalid number of parameters\n./ind8.run [fileName] [fileName] [N threads] [resFile]");
	}
	
	threadCount = strtol(argv[3], NULL, 10);
	if (threadCount < 1 || errno == ERANGE) 
		failure("N threads must be > 0");
		
	//open a result file
	FILE* fres = fopen(argv[4], "wt");
	if (fres == NULL)
		failure("Can not create a file");
		
	//open a thread semaphore
	sem_unlink(SEM_THREAD_NAME);
	semThread = sem_open(SEM_THREAD_NAME, O_CREAT, 0777, threadCount);
	if (semThread == SEM_FAILED)
		failure("Error on create a semaphore");
	
	fileInfo bigger, smaller;	
	prepareFiles(argv[1], argv[2], &bigger, &smaller);

	plagiarismSearch(bigger, smaller, fres);

	//close semaphore
    if (sem_close(semThread) == -1)
		perror("Can not close a semaphore");
    if (sem_unlink(SEM_THREAD_NAME) == -1)
		perror("Can not unlink a semaphore");
	
	//close files
	if (fclose(bigger.fp) == EOF)
		perror("Can not close a file");
	if (fclose(smaller.fp) == EOF)
		perror("Can not close a file");
	if (fclose(fres) == EOF)
		perror("Can not close a file");
	
	return;
}
