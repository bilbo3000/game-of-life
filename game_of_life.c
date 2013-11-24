/**
 * Game of life. 
 * CSCE 835
 * Dongpu
 * 11/17/2013
 */

#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <assert.h> 
#include "mpi.h"

void UpdateSlice(int** board, int dimension, int sliceSize, int rank, int p);
void WriteToFile(int* result, int dimension);
 
void main(int argc, char* argv[]) {
    assert(argc == 2); 

    char * inputfilename = argv[1];  // input file name
    int rank;  // Rank of the process
    int p;  // Number of processes 
    int sendToTopTag = 100; 
    int sendToBottomTag = 200;  
    MPI_Status status;  // Receive status
    
    // Start MPI
    MPI_Init(&argc, &argv); 
    
    // Get process rank
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    // Get number of processes
    MPI_Comm_size(MPI_COMM_WORLD, &p);

    // File IO 
	FILE * inputfp = NULL; 
	inputfp = fopen(inputfilename, "r");
	
	if (inputfp == NULL) {
		fprintf(stderr, "Cannot open input file. \n");
		exit(1); 
	}

    // Read number of iterations and board dimension
	int iterations = 0; 
	int dimension = 0; 
	 
	fscanf(inputfp, "%d", &iterations); 
	fscanf(inputfp, "%d", &dimension);
	
	// Calculate slice size
	int sliceSize = 0;
	
	if (p <= 1) {
		sliceSize = dimension; 
	}
	else {
		sliceSize = dimension / p; 
	}
	
    // Create and initialize board 
    int i = 0; 
    int j = 0;
    int **board;
    
	board = (int**)malloc((dimension + 1) * sizeof(int*)); 

	for (i = 0; i <= dimension; i++) {
	    board[i] = (int*)malloc((dimension + 1) * sizeof(int)); 
	    memset(board[i], 0, (dimension + 1) * sizeof(int)); 
	}
	
    // Read and populate the initial data
	int x = 0; 
	int y = 0;
	
	while (fscanf(inputfp, "%d,%d", &x, &y) != EOF) {
        if (x >= 1 && x <= dimension && y >= 1 && y <= dimension) {
            board[x][y] = 1; 
        }
	}	 
	  
	int topEdge = rank * sliceSize + 1; 
	int bottomEdge = rank == p - 1 ? dimension : (rank + 1) * sliceSize;  
	int dest1 = rank == 0 ? p - 1 : rank - 1;  // Neighbor above
	int dest2 = rank == p - 1 ? 0 : rank + 1;  // Neighbor below
	int row1 = topEdge == 1 ? dimension : topEdge - 1;  // Row above slice
	int row2 = bottomEdge == dimension ? 1 : bottomEdge + 1;  //Row below slice
		
	// Run the algorithm 
	while (iterations > 0) {
		// Update current slice
		UpdateSlice(board, dimension, sliceSize, rank, p); 
		 
		MPI_Barrier(MPI_COMM_WORLD);
		
		if (rank % 2 == 0) {  // Even number processors 
			// Send down
			MPI_Send(board[bottomEdge], dimension + 2, MPI_INT, dest2, sendToBottomTag, MPI_COMM_WORLD);
			
			// Receive from down
			MPI_Recv(board[row2], dimension + 2, MPI_INT, dest2, sendToTopTag, MPI_COMM_WORLD, &status);
			
			// Send up
			MPI_Send(board[topEdge], dimension + 2, MPI_INT, dest1, sendToTopTag, MPI_COMM_WORLD);
			
			// Receive from up 
			MPI_Recv(board[row1], dimension + 2, MPI_INT, dest1, sendToBottomTag, MPI_COMM_WORLD, &status);
		}
		else {  // Odd number processors 
			// Receive from up
			MPI_Recv(board[row1], dimension + 2, MPI_INT, dest1, sendToBottomTag, MPI_COMM_WORLD, &status);
		
			// Send up 
			MPI_Send(board[topEdge], dimension + 2, MPI_INT, dest1, sendToTopTag, MPI_COMM_WORLD);
			
			// Receive from down
			MPI_Recv(board[row2], dimension + 2, MPI_INT, dest2, sendToTopTag, MPI_COMM_WORLD, &status);
			
			// Send down
			MPI_Send(board[bottomEdge], dimension + 2, MPI_INT, dest2, sendToBottomTag, MPI_COMM_WORLD);
		}
		
		// Send updates to neighbors
		// MPI_Send(board[topEdge], dimension + 2, MPI_INT, dest1, sendToTopTag, MPI_COMM_WORLD);  
		// MPI_Send(board[bottomEdge], dimension + 2, MPI_INT, dest2, sendToBottomTag, MPI_COMM_WORLD);

		// Receive updates from neighbors 
		// MPI_Recv(board[row1], dimension + 2, MPI_INT, dest1, sendToBottomTag, MPI_COMM_WORLD, &status);
		// MPI_Recv(board[row2], dimension + 2, MPI_INT, dest2, sendToTopTag, MPI_COMM_WORLD, &status);
		
		MPI_Barrier(MPI_COMM_WORLD); 
		
		iterations--; 
	}
	 
	// Collect output result from workers 
	if (rank == 0) {  // root  
		int *result = (int*)malloc(dimension * dimension * sizeof(int));  
		memset(result, 0, dimension * dimension * sizeof(int));     
		int source;
		int *scounts = (int*)malloc(p * sizeof(int)); // Number of bytes send by each process
		
		// Create an array of receiving buffer size in byte
		for(i = 0; i < p; i++) {
			int topRow = i * sliceSize + 1; 
			int bottomRow = i == p - 1 ? dimension : (i + 1) * sliceSize;
			scounts[i] = (bottomRow - topRow + 1) * dimension * sizeof(int);
		}
		
		// Copy root to result
		int *resultPtr = result;
		for (i = topEdge; i <= bottomEdge; i++) {
			for (j = 1; j <= dimension; j++) {
				*resultPtr = board[i][j]; 
				resultPtr++; 
			}
		}
		
		// Receive results from each process 
		for (source = 1; source < p; source++) {
			int *procResult = (int*)malloc(scounts[source]);
			memset(procResult, 0, scounts[source]); 
			MPI_Recv(procResult, scounts[source], MPI_INT, source, 0, MPI_COMM_WORLD, &status); 
			memcpy(resultPtr, procResult, scounts[source]);
			resultPtr += scounts[source] / sizeof(int); 
			free(procResult); 
		}
		
		// Write final results to output file 
		WriteToFile(result, dimension);
	}
	else {  // Worker 
		int *sendBuf = (int*)malloc((bottomEdge - topEdge + 1) * dimension * sizeof(int)); 
		memset(sendBuf, 0, (bottomEdge - topEdge + 1) * dimension * sizeof(int));
		 
		int *curr = sendBuf; 

		for (i = topEdge; i <= bottomEdge; i++) {
			for (j = 1; j <= dimension; j++) {
				*curr = board[i][j]; 
				curr++; 
			}
		}
		
		MPI_Send(sendBuf, (bottomEdge - topEdge + 1) * dimension, MPI_INT, 0, 0, MPI_COMM_WORLD); 
		free(sendBuf);
	} 
	
    // Close files
	fclose(inputfp); 
	// fclose(outputfp);   	

    // Free board memory 
    for (i = 0; i <= dimension; i++) {
        free(board[i]); 
    }

    free(board); 
    
    // Shut down MPI
    MPI_Finalize(); 
    
	return; 
}

/*
 * Write results to output file. 
 */ 
void WriteToFile(int* result, int dimension) {
	int i = 0; 
	int j = 0; 
	char * outputfilename = "output.txt"; 
	FILE * outputfp = fopen(outputfilename, "w"); 
	
	for (i = 1; i <= dimension; i++) {
		for(j = 1; j <= dimension; j++) {
			if (*result == 1) {
				fprintf(outputfp, "%d,%d\n", i, j); 
			}
			
			result++; 
		}
	}
	
	// Close output file 
	fclose(outputfp); 
}

/*
 * Update a slice.  
 */
void UpdateSlice(int** board, int dimension, int sliceSize, int rank, int p) {
    int i = 0; 
    int j = 0; 
	int startRow = rank * sliceSize + 1; 
	int endRow = rank == p - 1 ? dimension : (rank + 1) * sliceSize; 
	
	int** temp = (int**)malloc((dimension + 1) * sizeof(int*)); 
	for (i = 0; i <= dimension; i++) {
		temp[i] = (int*)malloc((dimension + 1) * sizeof(int)); 
		memset(temp[i], 0, (dimension + 1) * sizeof(int)); 
	}
	
	// Locate changes
    for (i = startRow; i <= endRow; i++) {
        for (j = 1; j <= dimension; j++) {
            int livecnt = 0; 
            int a; 
            int b; 

            // Right
            a = i; 
            b = j == dimension ? 1 : j + 1; 
            livecnt += board[a][b]; 

            // Right bottom 
            a = i == dimension ? 1 : i + 1; 
            b = j == dimension ? 1 : j + 1; 
            livecnt += board[a][b]; 

            // Bottom 
            a = i == dimension ? 1 : i + 1; 
            b = j; 
            livecnt += board[a][b]; 

            // Left bottom 
            a = i == dimension ? 1 : i + 1; 
            b = j == 1 ? dimension : j - 1; 
            livecnt += board[a][b]; 

            // Left 
            a = i; 
            b = j == 1 ? dimension : j - 1; 
            livecnt += board[a][b]; 

            // Left top 
            a = i == 1 ? dimension : i - 1; 
            b = j == 1 ? dimension : j - 1; 
            livecnt += board[a][b]; 

            // Top 
            a = i == 1 ? dimension : i - 1; 
            b = j; 
            livecnt += board[a][b]; 

            // Right top 
            a = i == 1 ? dimension : i - 1; 
            b = j == dimension ? 1 : j + 1; 
            livecnt += board[a][b]; 
             
            // Update 
            if (board[i][j] == 1) {  // Live cell
            	if (livecnt < 2 || livecnt > 3) {
            		temp[i][j] = 1;  // Dies because of under or over populated
            	}
            }
            else {  // Deal cell
            	if (livecnt == 3) {
            		temp[i][j] = 1;  // Live with three neighbors 
            	}
            }
        }    
    } // end for
    
    // Update
    for (i = 1; i <= dimension; i++) {
    	for (j = 1; j <= dimension; j++) {
    		if (temp[i][j]) {
    			board[i][j] ^= 1; // Toggle the value
    		} 
    	}
    }
}
