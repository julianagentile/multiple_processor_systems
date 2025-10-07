//This file contains the code that the master process will execute.

#include <iostream>
#include <mpi.h>
#include <math.h>
#include <queue>
#include "RayTrace.h"
#include "slave.h"

void slaveMain(ConfigData* data)
{
    //Depending on the partitioning scheme, different things will happen.
    //You should have a different function for each of the required 
    //schemes that returns some values that you need to handle.
    switch (data->partitioningMode)
    {
        case PART_MODE_NONE:
            //The slave will do nothing since this means sequential operation.
            break;

        case PART_MODE_STATIC_STRIPS_VERTICAL:
            staticStripsVerticalSlave(data);
            break;

        case PART_MODE_STATIC_BLOCKS:
            staticSquareBlocksSlave(data);
            break;

        case PART_MODE_STATIC_CYCLES_HORIZONTAL:
            slaveStaticCyclesHorizontal(data);
            break;

        case PART_MODE_DYNAMIC:
            dynamicSlave(data);
            break;

        default:
            std::cout << "This mode (" << data->partitioningMode;
            std::cout << ") is not currently implemented." << std::endl;
            break;
    }
}

void dynamicSlave(ConfigData* data){
    int blockUnit[4];
    MPI_Status status;

    while (true){
        MPI_Send(NULL, 0, MPI_CHAR, 0, 1, MPI_COMM_WORLD);

        // get work unit!
        MPI_Recv(blockUnit, 4, MPI_INT, 0, 2, MPI_COMM_WORLD, &status);

        int startRow = blockUnit[0];
        int startCol = blockUnit[1];
        int blockHeight = blockUnit[3];
        int blockWidth = blockUnit[2];

        if (blockWidth == 0 && blockHeight == 0){
            break; // sign to terminate program
        }

        float* buffer = new float[3 * blockWidth * blockHeight + 1];

        double startTime = MPI_Wtime();

        for (int i = 0; i < blockHeight; ++i) {
            for (int j = 0; j < blockWidth; ++j) {
                int row = startRow + i;
                int col = startCol + j;
                int idx = 3 * (i * blockWidth + j);
                shadePixel(&buffer[idx], row, col, data);
            }
        }

        double endTime = MPI_Wtime();
        double computationTime = endTime - startTime;
        buffer[blockWidth * blockHeight * 3] = computationTime;

        // Send result back to master
        MPI_Send(buffer, 3 * blockWidth * blockHeight + 1, MPI_FLOAT, 0, 3, MPI_COMM_WORLD);

        delete[] buffer;
    }
}


// void staticStripsHorizontalSlave(ConfigData* data){

//     // dividing by columns
//     int rows = data->height / data->mpi_procs;  // divide height into strips
//     int extra = data->height % data->mpi_procs; // extra rows for some processes

//     // Determine the first and last row this process is responsible for
//     int firstRow = data->mpi_rank * rows - 1;
//     int lastRow = firstRow + rows - 1;

//     if (data -> mpi_rank == data -> mpi_procs -1 ){
//         lastRow += extra; 
//     }

//     // only need to allocate the memroy for the processe's portion
//     int numRows = lastRow - firstRow + 1;
//     float* pixels = new float[3 * numRows * data->width];
//     // Start the computation timer
//     double computationStart = MPI_Wtime();

//     std::cout << "Rendering a Scene in Slave Process: " << std::endl;
//     // Render the scene for the assigned strip of columns
//     for (int i = firstRow; i <= lastRow; ++i) {
//         for (int j = 0; j < data->width; ++j) {
//             int row = i;
//             int column = j;

//             int baseIndex = 3 * ((i - firstRow) * data->width + column);
//             shadePixel(&(pixels[baseIndex]), row, column, data);
//         }
//     }

//     // Stop the computation timer
//     double computationStop = MPI_Wtime();
//     double computationTime = computationStop - computationStart;

//     // Send the results to the master
//     MPI_Send(pixels, 3 * numRows * data->width, MPI_FLOAT, 0, 0, MPI_COMM_WORLD);

//     // Print the times for this slave
//     std::cout << "Slave " << data->mpi_rank << " Computation Time: " << computationTime << " seconds" << std::endl;

//     delete[] pixels;
// }

void staticStripsVerticalSlave(ConfigData* data){

    // dividing by columns
    int cols = data->width / data->mpi_procs;  // divide height into strips
    int extra = data->width % data->mpi_procs; // extra columns for some processes
    int firstCol = data->mpi_rank * cols;
    int lastCol = firstCol + cols - 1;

    // // give last rank the extra data to compute
    if (data -> mpi_rank == data -> mpi_procs -1 ){
        lastCol += extra; 
    }
    std::cout << "Rank " << data->mpi_rank << " processes columns from " << firstCol << " to " << lastCol << std::endl;

    // only need to allocate the memroy for the processe's portion
    int numCols = lastCol - firstCol + 1;
    float* pixelColumns = new float[3 * data->height * numCols];
    
    double computationStart = MPI_Wtime();

    for (int j = firstCol; j <= lastCol; ++j) {
        for (int i = 0; i < data->height; ++i) {
            int baseIndex = 3 * (i * numCols + (j - firstCol));
            shadePixel(&(pixelColumns[baseIndex]), i, j, data);
        }
    }

    // Stop the computation timer
    double computationStop = MPI_Wtime();
    double computationTime = computationStop - computationStart;
    pixelColumns[3 * data->height * numCols] = computationTime;
    // count = 3(RGB) * data->height (number of rows) * numCols (nuber of cols in this process)
    MPI_Send(pixelColumns, (3 * data->height * numCols) + 1, MPI_FLOAT, 0, 100, MPI_COMM_WORLD);
    delete[] pixelColumns;
}

void staticSquareBlocksSlave(ConfigData* data){

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

    // only need to allocate the memory for the process's portion
    int sizeP = (3 * (lastRow - firstRow + 1) * (lastCol - firstCol + 1)) + 1;
    float* pixelSquares = new float[sizeP];
    
    double computationStart = MPI_Wtime();

    for (int i = 0; i < (lastRow - firstRow + 1); i++) {
        for (int j = 0; j < (lastCol - firstCol + 1); j++) {
            int baseIndex = 3 * (i * (lastCol - firstCol + 1) + j);
            int x = i + firstRow;
            int y = j + firstCol;
            if(x < (data->width - 1) && y < (data->height - 1) && baseIndex < sizeP){
                shadePixel(&(pixelSquares[baseIndex]), x, y, data);
            }
        }
    }

    // Stop the computation timer
    double computationStop = MPI_Wtime();
    double computationTime = computationStop - computationStart;
    pixelSquares[3 * (lastRow - firstRow + 1) * (lastCol - firstCol + 1)] = computationTime;

    // In staticSquareBlocksSlave, before MPI_Send:
    std::cout << "Slave " << data->mpi_rank << ": Sending data in square [" << firstCol << ", " << firstRow << "] to ["
            << lastCol << ", " << lastRow << "]" << std::endl;
    std::cout << "Slave " << data->mpi_rank << ": First few values: " << pixelSquares[0] << ", " << pixelSquares[3] << ", " << pixelSquares[6] << std::endl;
    // count = 3(RGB) * data->height (number of rows) * numCols (nuber of cols in this process)
    MPI_Send(pixelSquares, (3 * (lastRow - firstRow + 1) * (lastCol - firstCol + 1)) + 1, MPI_FLOAT, 0, 100, MPI_COMM_WORLD);
  

    std::cout << "Slave " << data->mpi_rank << " Computation Time: " << computationTime << " seconds" << std::endl;

    delete[] pixelSquares;
}


void slaveStaticCyclesHorizontal(ConfigData* data) {
    // Number of rows per cycle block
    int blockHeight = data->cycleSize;

    // We'll collect the rows this process is responsible for
    std::vector<int> ownedRows;

    // Each process takes every `mpi_procs`-th block of `blockHeight` rows
    for (int startRow = data->mpi_rank * blockHeight; startRow < data->height; startRow += blockHeight * data->mpi_procs) {
        for (int r = 0; r < blockHeight; ++r) {
            int row = startRow + r;
            if (row < data->height) {
                ownedRows.push_back(row);
            }
        }
    }

    std::cout << "Rank " << data->mpi_rank << " processes rows: ";
    for (int r : ownedRows) std::cout << r << " ";
    std::cout << std::endl;

    // Only need to allocate memory for the process's rows across full width
    int numRows = ownedRows.size();
    float* pixelRows = new float[3 * data->width * numRows + 1];

    double computationStart = MPI_Wtime();

    for (int idx = 0; idx < numRows; ++idx) {
        int i = ownedRows[idx];
        for (int j = 0; j < data->width; ++j) {
            int baseIndex = 3 * (idx * data->width + j);
            shadePixel(&(pixelRows[baseIndex]), i, j, data);
        }
    }

    double computationStop = MPI_Wtime();
    double computationTime = computationStop - computationStart;
    pixelRows[3 * data->width * numRows] = computationTime;

    // count = 3(RGB) * data->width * numRows + 1 for time
    MPI_Send(pixelRows, (3 * data->width * numRows) + 1, MPI_FLOAT, 0, 100, MPI_COMM_WORLD);

    delete[] pixelRows;
}
