//This file contains the code that the master process will execute.

#include <iostream>
#include <mpi.h>
#include <math.h>
#include <queue>
#include <map>
#include "RayTrace.h"

#include "master.h"

void masterMain(ConfigData* data)
{
    //Allocate space for the image on the master.
    float* pixels = new float[3 * data->width * data->height];
    
    //Execution time will be defined as how long it takes
    //for the given function to execute based on partitioning
    //type.
    double renderTime = 0.0, startTime, stopTime;

    switch (data->partitioningMode)
    
    {
        case PART_MODE_NONE:
            //Call the function that will handle this.
            startTime = MPI_Wtime();
            masterSequential(data, pixels);
            stopTime = MPI_Wtime();
            break;

        // Need to test on all case sizes (and add time measurements)
        case PART_MODE_STATIC_STRIPS_VERTICAL:
            startTime = MPI_Wtime();
            staticStripsVerticalMaster(data, pixels);
            stopTime = MPI_Wtime();
            break;

        // Eve
        case PART_MODE_STATIC_BLOCKS:
            startTime = MPI_Wtime();
            staticSquareBlocksMaster(data, pixels);
            stopTime = MPI_Wtime();
            break;
        // Juliana
        case PART_MODE_STATIC_CYCLES_HORIZONTAL:
            startTime = MPI_Wtime();
            masterStaticCyclesHorizontal();
            stopTime = MPI_Wtime();
            break;

        // Alex starting
        case PART_MODE_DYNAMIC:
            startTime = MPI_Wtime();
            dynamicMaster(data, pixels);
            stopTime = MPI_Wtime();
            break;    

        default:
            std::cout << "This mode (" << data->partitioningMode;
            std::cout << ") is not currently implemented." << std::endl;
            break;
    }

    renderTime = stopTime - startTime;
    std::cout << "Execution Time: " << renderTime << " seconds" << std::endl << std::endl;

    //After this gets done, save the image.
    std::cout << "Image will be save to: ";
    std::string file = "renders/" + generateFileName();
    std::cout << file << std::endl;
    savePixels(file, pixels, data);

    //Delete the pixel data.
    delete[] pixels; 
}

void dynamicMaster(ConfigData* data, float* pixels){

    // centralized single queue to split between slave processes (worker does no computations)
   // int numWorkers = data->mpi_procs -1;
    int blockWidth = data->dynamicBlockWidth;
    int blockHeight = data->dynamicBlockHeight;
    int imageWidth = data->width;
    int imageHeight = data->height;
    double communicationTime = 0.0;
    double computationTime = 0.0;

    // Centralized QUEUE
    std::queue<DynamicUnit> centralizeQueue;
    std::map<int, DynamicUnit> workInProgress;

    // create work units for worker processes
    for (int row = 0; row < imageHeight; row+=blockHeight){
        for(int col = 0; col < imageWidth; col+=blockWidth){
            DynamicUnit dynamicUnit;
            dynamicUnit.startRow = row;
            dynamicUnit.startCol = col;
            // overflow check
            dynamicUnit.blockHeight = std::min(blockHeight, imageHeight  - row);  
            dynamicUnit.blockWidth = std::min(blockWidth, imageWidth   - col);
            centralizeQueue.push(dynamicUnit);

        }
    }

    // variable to determine if all workers are complete & queue empty

    int completedWorkers = 0;
    MPI_Status status;

    // termination case
    while (completedWorkers < data->mpi_procs - 1){
        double commStart = MPI_Wtime();
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        double commEnd = MPI_Wtime();
        communicationTime += (commEnd - commStart);

        int rank = status.MPI_SOURCE;
        int tag = status.MPI_TAG;
    
        if(tag == 1) {
            double commStart2 = MPI_Wtime();
            MPI_Recv(NULL, 0, MPI_CHAR, rank, tag, MPI_COMM_WORLD, &status);
            double commEnd2 = MPI_Wtime();
            communicationTime += (commEnd2 - commStart2);

            if(!centralizeQueue.empty()) {
                DynamicUnit unit = centralizeQueue.front();
                centralizeQueue.pop();

                int msg[4] = {unit.startRow, unit.startCol, unit.blockWidth, unit.blockHeight};
                
                double commStart3 = MPI_Wtime();
                MPI_Send(msg, 4, MPI_INT, rank, 2, MPI_COMM_WORLD);
                double commEnd3 = MPI_Wtime();
                communicationTime += (commEnd3 - commStart3);

                workInProgress[rank] = unit;
            }
            else {
                int done[4] = {0, 0, 0, 0};

                double commStart4 = MPI_Wtime();
                MPI_Send(done, 4, MPI_INT, rank, 2, MPI_COMM_WORLD);
                double commEnd4 = MPI_Wtime();
                communicationTime += (commEnd4 - commStart4);

                completedWorkers ++; 
            }
        }
        else if(tag == 3) {
            DynamicUnit unit = workInProgress[rank];
            int size = (unit.blockWidth * unit.blockHeight * 3) + 1;
            float* tempBuffer = new float[size];

            double commStart5 = MPI_Wtime();
            MPI_Recv(tempBuffer, size, MPI_FLOAT, rank, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            double commEnd5 = MPI_Wtime();
            communicationTime += (commEnd5 - commStart5);
            float computeTime = tempBuffer[size - 1];

            computationTime += computeTime;
           
            for (int i = 0; i < unit.blockHeight; ++i) {
                for (int j = 0; j < unit.blockWidth; ++j) {
                    int masterIndex = 3 * ((unit.startRow + i) * data->width + (unit.startCol + j));
                    int bufferIndex = 3 * (i * unit.blockWidth + j);
                    pixels[masterIndex] = tempBuffer[bufferIndex];
                    pixels[masterIndex + 1] = tempBuffer[bufferIndex + 1];
                    pixels[masterIndex + 2] = tempBuffer[bufferIndex + 2];
                }
            }
    
            delete[] tempBuffer;
            workInProgress.erase(rank);

        }
    }
    std::cout << "Terminated: " << std::endl;

    std::cout << "Total Computation Time: " << computationTime << " seconds" << std::endl;
    std::cout << "Total Communication Time: " << communicationTime << " seconds" << std::endl;
    double c2cRatio = communicationTime / computationTime;
    std::cout << "C-to-C Ratio: " << c2cRatio << std::endl;
    
}



void staticStripsVerticalMaster(ConfigData* data, float* pixels){
    //Start the computation time timer.
    double computationTime = 0.0;
    double communicationTime = 0.0;

    int cols = data->width / data->mpi_procs;  // divide height into strips
    int extra = data->width % data->mpi_procs; // extra rows for some processes
    int firstCol = data->mpi_rank * cols;
    int lastCol = firstCol  + cols - 1;

    if (data->mpi_rank == data->mpi_procs - 1) {
        lastCol += extra;
    }

    double computeStart = MPI_Wtime();
    for (int i = 0; i < data->height; ++i) {
        for (int j = firstCol; j <= lastCol; ++j) {
            int baseIndex = 3 * (i * data->width + j);
            shadePixel(&(pixels[baseIndex]), i, j, data);
        }
    }
    double computeEnd = MPI_Wtime();
    double totalMasterTime = computeEnd - computeStart;
    computationTime += totalMasterTime;

    // receive rendered scenes
    for( int i = 1; i < data->mpi_procs; ++i )
    {
        int columnOne = i * cols;
        int columnFinish = columnOne + cols - 1;

        if (i == data->mpi_procs - 1) {
            columnFinish += extra;
        }

        float* tempBuffer = new float[3 * data->height * (columnFinish - columnOne + 1)];

        // Receive the data into tempBuffer
        double commStart1 = MPI_Wtime();
        MPI_Recv(tempBuffer, (3 * data->height * (columnFinish - columnOne + 1)) +1, MPI_FLOAT, i, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        double commEnd1 = MPI_Wtime();
        communicationTime += (commEnd1 - commStart1);

        float computeTime = tempBuffer[3 * data->height * (columnFinish - columnOne + 1)];
        computationTime += computeTime;

        int receivedCols = columnFinish - columnOne + 1;
        for (int j = 0; j < receivedCols; ++j) {
            for (int k = 0; k < data->height; ++k) {
                int masterIndex = 3 * (k * data->width + (columnOne + j));
                int slaveIndex = 3 * (k * receivedCols + j);
                pixels[masterIndex] = tempBuffer[slaveIndex];
                pixels[masterIndex + 1] = tempBuffer[slaveIndex + 1];
                pixels[masterIndex + 2] = tempBuffer[slaveIndex + 2];
            }
        }
        delete[] tempBuffer;
    }
        
     
    //Print the times and the c-to-c ratio
	//This section of printing, IN THIS ORDER, needs to be included in all of the
	//functions that you write at the end of the function.
    std::cout << "Total Computation Time: " << computationTime << " seconds" << std::endl;
    std::cout << "Total Communication Time: " << communicationTime << " seconds" << std::endl;
    double c2cRatio = communicationTime / computationTime;
    std::cout << "C-to-C Ratio: " << c2cRatio << std::endl;


}

void staticSquareBlocksMaster(ConfigData* data, float* pixels){

	double compTime = 0.0;
	double commTime = 0.0;

// dividing into square blocks
    int square = 0;
    int root = (int) sqrt(data->mpi_procs);
    float test = (float)data->mpi_procs / (float)root;
    if(test != root){
        square = ((int)sqrt(data->mpi_procs) + 1) * ((int)sqrt(data->mpi_procs) + 1);
    }else{
        square = data->mpi_procs;
    }

    int size = (data->width)*(data->height) / square;  // divide total number of elements into sqaures
    int dim = sqrt(size);

    int max = data->width / dim;
    int hOffset = (data->width - (dim * max));
    if(hOffset > 1){
        hOffset /= 2;
    }
    int vOffset = (data->height - (dim * max));
    if(vOffset > 1){
        vOffset /= 2;
    }

    int firstCol = (data->mpi_rank % max) * dim + hOffset;
    int lastCol = firstCol + dim - 1;
    int firstRow = (data->mpi_rank / max) * dim + vOffset;
    int lastRow = firstRow + dim - 1;

    // extend processes with edge squares to cover entire scene
    if (firstCol == hOffset){
        firstCol = 0;
    }
    if (lastCol == dim * max + hOffset){
        lastCol = data->width - 1;
    }
    if (firstRow == vOffset){
        firstRow = 0;
    }
    if (lastRow == dim * max + vOffset || (data->mpi_procs - data->mpi_rank - 1) < max){
        lastRow = data->height - 1;
    }
    std::cout << "Rank " << data->mpi_rank << " processes data in square [" << firstCol << ", " << firstRow << "] to ["
            << lastCol << ", " << lastRow << "]" << std::endl;
    
    //Start the computation time timer.
    double compStart = MPI_Wtime();

    for (int i = 0; i < (lastRow - firstRow + 1); i++) {
        for (int j = 0; j < (lastCol - firstCol + 1); j++) {
            int baseIndex = 3 * ((i + firstRow) * data->width + (j + firstCol));
            int x = i + firstRow;
            int y = j + firstCol;
            if(x < (data->width - 1) && y < (data->height - 1)){
                shadePixel(&(pixels[baseIndex]), x, y, data);
            }
        }
    }
    double compEnd = MPI_Wtime();
    double masterTime = compEnd - compStart;
    compTime += masterTime;

    // receive rendered scenes
    for(int n = 1; n < data->mpi_procs; n++)
    {
        firstCol = (n % max) * dim + hOffset;
        lastCol = firstCol + dim - 1;
        firstRow = (n / max) * dim + vOffset;
        lastRow = firstRow + dim - 1;

        if (firstCol == hOffset){
            firstCol = 0;
        }
        if (lastCol == dim * max + hOffset){
            lastCol = data->width - 1;
        }
        if (firstRow == vOffset){
            firstRow = 0;
        }
        if (lastRow == dim * max + vOffset || (data->mpi_procs - n - 1) < max){
            lastRow = data->height - 1;
        }

        int size = (3 * (lastRow - firstRow + 1) * (lastCol - firstCol + 1)) + 1;
        float* tempBuffer = new float[size];
        // Receive the data into tempBuffer
	double commStart = MPI_Wtime();
        MPI_Recv(tempBuffer, (3 * (lastRow - firstRow + 1) * (lastCol - firstCol + 1)) + 1, MPI_FLOAT, n, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	double commEnd = MPI_Wtime();
	commTime += commEnd - commStart;

	float compTimeR= tempBuffer[3 * (lastRow - firstRow + 1) * (lastCol - firstCol + 1)];
	compTime += compTimeR;

        int masterIndex = 0;
        int slaveIndex = 0;
        for (int i = 0; i < (lastRow - firstRow + 1); i++) {
            for (int j = 0; j < (lastCol - firstCol + 1); j++) {
                masterIndex = 3 * ((i + firstRow) * data->width + (j + firstCol));
                slaveIndex = 3 * (i * (lastCol - firstCol + 1) + j);
                if(slaveIndex < size - 1){
                    pixels[masterIndex] = tempBuffer[slaveIndex];
                    pixels[masterIndex + 1] = tempBuffer[slaveIndex + 1];
                    pixels[masterIndex + 2] = tempBuffer[slaveIndex + 2];
                }
            }
        }
        delete[] tempBuffer;
    }

    //Print the times and the c-to-c ratio
	//This section of printing, IN THIS ORDER, needs to be included in all of the
	//functions that you write at the end of the function.
    std::cout << "Total Computation Time: " << compTime << " seconds" << std::endl;
    std::cout << "Total Communication Time: " << commTime << " seconds" << std::endl;
    double c2cRatio = commTime / compTime;
    std::cout << "C-to-C Ratio: " << c2cRatio << std::endl;


}


void masterStaticCyclesHorizontal(ConfigData* data, float* pixels)
{
    double computationTime = 0.0;
    double communicationTime = 0.0;

    int width = data->width;
    int height = data->height;
    int rank = data->mpi_rank;
    int size = data->mpi_procs;

    // Calculate how many rows this process will render
    std::vector<int> localRows;
    for (int startRow = rank * data->cycleSize; startRow < height; startRow += data->cycleSize * size) {
        for (int r = 0; r < data->cycleSize; ++r) {
            int row = startRow + r;
            if (row < height) localRows.push_back(row);
        }
    }
    // Exit early if this rank has no rows and it's not the master
    if (localRows.empty() && rank != 0) return;



    double computeStart = MPI_Wtime();

    // Render local rows
    float* localPixels = new float[3 * localRows.size() * width + 1];
    for (size_t i = 0; i < localRows.size(); ++i) {
        int row = localRows[i];
        for (int col = 0; col < width; ++col) {
            int index = 3 * (i * width + col);
            shadePixel(&localPixels[index], row, col, data);
        }
    }

    double computeEnd = MPI_Wtime();
    computationTime += (computeEnd - computeStart);

    if (rank == 0) {
        // Master copies its own results
        for (size_t i = 0; i < localRows.size(); ++i) {
            int row = localRows[i];
            for (int col = 0; col < width; ++col) {
                int srcIndex = 3 * (i * width + col);
                int dstIndex = 3 * (row * width + col);
                pixels[dstIndex + 0] = localPixels[srcIndex + 0];
                pixels[dstIndex + 1] = localPixels[srcIndex + 1];
                pixels[dstIndex + 2] = localPixels[srcIndex + 2];
            }
        }

        // Receive from other processes
        for (int src = 1; src < size; ++src) {
            // Calculate number of rows for this process
            std::vector<int> recvRows;
            for (int startRow = src * data->cycleSize; startRow < height; startRow += data->cycleSize * size) {
                for (int r = 0; r < data->cycleSize; ++r) {
                    int row = startRow + r;
                    if (row < height) recvRows.push_back(row);
                }
            }

            int recvCount = recvRows.size() * width * 3 + 1;
            float* recvBuffer = new float[recvCount];

            double commStart = MPI_Wtime();
            MPI_Recv(recvBuffer, recvCount, MPI_FLOAT, src, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            double commEnd = MPI_Wtime();
            communicationTime += (commEnd - commStart);

            // Copy received data into the final pixel buffer
            for (size_t i = 0; i < recvRows.size(); ++i) {
                int row = recvRows[i];
                for (int col = 0; col < width; ++col) {
                    int srcIndex = 3 * (i * width + col);
                    int dstIndex = 3 * (row * width + col);
                    pixels[dstIndex + 0] = recvBuffer[srcIndex + 0];
                    pixels[dstIndex + 1] = recvBuffer[srcIndex + 1];
                    pixels[dstIndex + 2] = recvBuffer[srcIndex + 2];
                }
            }

            delete[] recvBuffer;
        }
    } else {
        // Send data to master
        int sendCount = localRows.size() * width * 3;
        double commStart = MPI_Wtime();
        MPI_Send(localPixels, sendCount, MPI_FLOAT, 0, 100, MPI_COMM_WORLD);
        double commEnd = MPI_Wtime();
        communicationTime += (commEnd - commStart);
    }

    delete[] localPixels;

    //Print the times and the c-to-c ratio
        //This section of printing, IN THIS ORDER, needs to be included in all of the
        //functions that you write at the end of the function.
    std::cout << "Total Computation Time: " << computationTime << " seconds" << std::endl;
    std::cout << "Total Communication Time: " << communicationTime << " seconds" << std::endl;
    double c2cRatio = communicationTime / computationTime;
    std::cout << "C-to-C Ratio: " << c2cRatio << std::endl;
}



void masterSequential(ConfigData* data, float* pixels)
{
    //Start the computation time timer.
    double computationStart = MPI_Wtime();

    //Render the scene.
    for( int i = 0; i < data->height; ++i )
    {
        for( int j = 0; j < data->width; ++j )
        {
            int row = i;
            int column = j;

            //Calculate the index into the array.
            int baseIndex = 3 * ( row * data->width + column );

            //Call the function to shade the pixel.
            shadePixel(&(pixels[baseIndex]),row,j,data);
        }
    }

    //Stop the comp. timer
    double computationStop = MPI_Wtime();
    double computationTime = computationStop - computationStart;

    //After receiving from all processes, the communication time will
    //be obtained.
    double communicationTime = 0.0;

    //Print the times and the c-to-c ratio
	//This section of printing, IN THIS ORDER, needs to be included in all of the
	//functions that you write at the end of the function.
    std::cout << "Total Computation Time: " << computationTime << " seconds" << std::endl;
    std::cout << "Total Communication Time: " << communicationTime << " seconds" << std::endl;
    double c2cRatio = communicationTime / computationTime;
    std::cout << "C-to-C Ratio: " << c2cRatio << std::endl;
}

// void staticStripsHorizontalMaster(ConfigData* data, float* pixels){
//     //Start the computation time timer.
//     double computationStart = MPI_Wtime();

//     int rows = data->height / data->mpi_procs;  // divide height into strips
//     int extra = data->height % data->mpi_procs; // extra rows for some processes

//     int firstRow = 0;
//     int lastRow = rows - 1;
//     if (data->mpi_rank == data->mpi_procs - 1) {
//         lastRow += extra;
//     }

//     for (int i = firstRow; i <= lastRow; ++i) {
//         for (int j = 0; j < data->width; ++j) {
//             int baseIndex = 3 * (i * data->width + j);
//             shadePixel(&(pixels[baseIndex]), i, j, data);
//         }
//     }

//     // receive rendered scenes
//     for( int i = 1; i < data->mpi_procs; ++i )
//     {
//         int firstRow = i * rows;
//         int lastRow = firstRow + rows - 1;

//         if (i == data->mpi_procs - 1) {
//             lastRow += extra;
//         }

//         // Ensure lastRow does not exceed image bounds
//         lastRow = std::min(lastRow, data->height - 1);

//         // Receive data from each slave
//         MPI_Recv(pixels + (3 * firstRow * data->width), 
//                  3 * (lastRow - firstRow + 1) * data->width, 
//                  MPI_FLOAT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
//     }

//     //Stop the comp. timer
//     double computationStop = MPI_Wtime();
//     double computationTime = computationStop - computationStart;

//     //After receiving from all processes, the communication time will
//     //be obtained.
//     double communicationTime = computationTime;

//     //Print the times and the c-to-c ratio
// 	//This section of printing, IN THIS ORDER, needs to be included in all of the
// 	//functions that you write at the end of the function.
//     std::cout << "Total Computation Time: " << computationTime << " seconds" << std::endl;
//     std::cout << "Total Communication Time: " << communicationTime << " seconds" << std::endl;
//     double c2cRatio = communicationTime / computationTime;
//     std::cout << "C-to-C Ratio: " << c2cRatio << std::endl;

// }
