/**
 * Author: Lukas Dibdak
 * Project: Mesh multiplication
 * FIT VUT 2016/17
 */

#include <iostream>
#include <queue>
#include <vector>
#include <deque>
#include <fstream>
#include <mpi.h>
#include <term.h>

using namespace std;

const int FIRST_PROCESS = 0;

const int REG_A = 100;
const int REG_B = 101;
const int REG_C = 102;

const int ROW_NUMBERS = 20;
const int COL_NUMBERS = 30;

const int ROW_TYPE = 0;
const int COLUMN_TYPE = 1;

const int ERROR = -1;

/** Main class representing matrix **/
class Matrix {
private:
    int type;
    int equalSize;
    int rowCount, colCount;

    bool failed;

    deque<deque<int> > numbers;

public:
    Matrix();

    Matrix(const int type);

    int getRowCount();

    int getColumnCount();

    int getEqualSize();

    bool controlMatrixSize();

    bool isFailed();

    void convertColumnMatrix();

    void setSizeCount(int number);

    void setSize(int rowCount, int colCount);

    void setEqualSize(int equalSize);

    void storeNumberVector(deque<int> number);

    void storeNumbersFromFile(const char *fileName);

    vector<int> getNextNumbers();

    deque<int> getNextNumbersDeque();
};

struct {
    int A;
    int B;
    int C;
} Registers;

bool isNumber(char character) {
    return '0' <= character && character <= '9';
}

bool isFirstColumnProcessor(int columnCount, int processRank) {
    return (processRank % columnCount) == 0;
}

bool isFirstRowProcessor(int columnCount, int processRank) {
    return processRank < columnCount;
}

int getRowUpProcessor(int columnCount, int processRank) {
    return processRank - columnCount;
}

int getRowDownProcessor(int columnCount, int processRank) {
    return processRank + columnCount;
}

int getColumnLeftProcessor(int processRank) {
    return processRank - 1;
}

int getColumnRightProcessor(int processRank) {
    return processRank + 1;
}

bool isLastRowProcessor(int columnCount, int row, int processRank) {
    return processRank >= (columnCount * (row - 1));
}

bool isLastColumnProcessor(int columnCount, int processRank) {
    return ((processRank + 1) % columnCount) == 0;
}

int main(int argc, char *argv[]) {
    /** MPI Initialization **/
    MPI_Init(&argc, &argv);

    int numberOfProcesses;
    MPI_Comm_size(MPI_COMM_WORLD, &numberOfProcesses);

    int processRank;
    MPI_Comm_rank(MPI_COMM_WORLD, &processRank);

    Registers.C = 0;

    int rowCount, columnCount, equalCount;
    deque<int> rowNumbers, colNumbers;

    /** File parsing **/
    if (processRank == FIRST_PROCESS) {
        Matrix *matrixA = new Matrix(ROW_TYPE);
        Matrix *matrixB = new Matrix(COLUMN_TYPE);
        Matrix *matrix = new Matrix();

        matrixA->storeNumbersFromFile("mat1");
        matrixB->storeNumbersFromFile("mat2");


        if (!matrixA->controlMatrixSize() || matrixA->isFailed()) {
            cerr << "Matrix 1 is not correct." << endl;
            MPI_Abort(MPI_COMM_WORLD, ERROR);
        } else if (!matrixB->controlMatrixSize() || matrixB->isFailed()) {
            cerr << "Matrix 2 is not correct." << endl;
            MPI_Abort(MPI_COMM_WORLD, ERROR);
        } else if (matrixA->getColumnCount() != matrixB->getRowCount()) {
            cerr << "Matrix 1 column count does not match to matrix 2 row count." << endl;
            MPI_Abort(MPI_COMM_WORLD, ERROR);
        }

        matrixB->convertColumnMatrix();

        matrix->setSize(matrixA->getRowCount(), matrixB->getColumnCount());
        matrix->setEqualSize(matrixA->getColumnCount());

        rowCount = matrix->getRowCount();
        columnCount = matrix->getColumnCount();
        equalCount = matrix->getEqualSize();

        /** Sending other processors matrix data **/
        int numbers[] = {rowCount, columnCount, equalCount};
        MPI_Bcast(numbers, 3, MPI_INT, FIRST_PROCESS, MPI_COMM_WORLD);

        rowNumbers = matrixA->getNextNumbersDeque();
        colNumbers = matrixB->getNextNumbersDeque();

        for (int i = 1; i < matrix->getRowCount(); i++) {
            vector<int> toSend = matrixA->getNextNumbers();
            MPI_Send(toSend.data(), matrix->getEqualSize(), MPI_INT, i * matrix->getColumnCount(), ROW_NUMBERS, MPI_COMM_WORLD);
        }

        for (int i = 1; i < matrix->getColumnCount(); i++) {
            vector<int> toSend = matrixB->getNextNumbers();
            MPI_Send(toSend.data(), matrix->getEqualSize(), MPI_INT, i, COL_NUMBERS, MPI_COMM_WORLD);
        }
    } else {
        int *number = (int *) malloc(sizeof(int) * 3);
        MPI_Bcast(number, 3, MPI_INT, FIRST_PROCESS, MPI_COMM_WORLD);

        rowCount = number[0];
        columnCount = number[1];
        equalCount = number[2];

        if (processRank % columnCount == 0) {
            int *numbers = (int *) malloc(sizeof(int) * equalCount);
            MPI_Recv(numbers, equalCount, MPI_INT, FIRST_PROCESS, ROW_NUMBERS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (int i = 0; i < equalCount; ++i) {
                rowNumbers.push_back(numbers[i]);
            }
        } else if (processRank < columnCount) {
            int *numbers = (int *) malloc(sizeof(int) * equalCount);
            MPI_Recv(numbers, equalCount, MPI_INT, FIRST_PROCESS, COL_NUMBERS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (int i = 0; i < equalCount; ++i) {
                colNumbers.push_back(numbers[i]);
            }
        }
    }

    /** Main loop **/
    for (int i = 0; i < equalCount; i++) {
        if (isFirstColumnProcessor(columnCount, processRank) && isFirstRowProcessor(columnCount, processRank)) {
            Registers.A = rowNumbers.front();
            rowNumbers.pop_front();

            Registers.B = colNumbers.front();
            colNumbers.pop_front();

        } else if (isFirstColumnProcessor(columnCount, processRank)) {
            Registers.A = rowNumbers.front();
            rowNumbers.pop_front();

            int number;
            MPI_Recv(&number, 1, MPI_INT, getRowUpProcessor(columnCount, processRank), REG_B, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            Registers.B = number;

        } else if (isFirstRowProcessor(columnCount, processRank)) {
            Registers.B = colNumbers.front();
            colNumbers.pop_front();

            int number;
            MPI_Recv(&number, 1, MPI_INT, getColumnLeftProcessor(processRank), REG_A, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            Registers.A = number;

        } else {
            int numberA;
            MPI_Recv(&numberA, 1, MPI_INT, getColumnLeftProcessor(processRank), REG_A, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            Registers.A = numberA;

            int numberB;
            MPI_Recv(&numberB, 1, MPI_INT, getRowUpProcessor(columnCount, processRank), REG_B, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            Registers.B = numberB;
        }

        Registers.C += Registers.A * Registers.B;

        if (!isLastColumnProcessor(columnCount, processRank)) {
            int A = Registers.A;
            MPI_Send(&A, 1, MPI_INT, getColumnRightProcessor(processRank), REG_A, MPI_COMM_WORLD);
        }

        if (!isLastRowProcessor(columnCount, rowCount, processRank)) {
            int B = Registers.B;
            MPI_Send(&B, 1, MPI_INT, getRowDownProcessor(columnCount, processRank), REG_B, MPI_COMM_WORLD);
        }
    }

    /** Printing output matrix **/
    if (processRank == FIRST_PROCESS) {
        cout << rowCount << ":" << columnCount << endl;
        cout << Registers.C;

        if (isLastColumnProcessor(columnCount, processRank)) {
            cout << endl;
        } else {
            cout << " ";
        }

        for (int i = 1; i < numberOfProcesses; i++) {
            int number;
            MPI_Recv(&number, 1, MPI_INT, i, REG_C, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            cout << number;
            if (isLastColumnProcessor(columnCount, i)) {
                cout << endl;
            } else {
                cout << " ";
            }
        }
    } else {
        int C = Registers.C;
        MPI_Send(&C, 1, MPI_INT, FIRST_PROCESS, REG_C, MPI_COMM_WORLD);
    }

    MPI_Finalize();

    return 0;
}

int Matrix::getRowCount() {
    return rowCount;
}

int Matrix::getColumnCount() {
    return colCount;
}

Matrix::Matrix() {
    failed = false;
}

Matrix::Matrix(const int type) {
    Matrix::type = type;
    failed = false;
}

void Matrix::setSizeCount(int number) {
    if (type == ROW_TYPE) {
        rowCount = number;
        colCount = -1;
    } else {
        colCount = number;
        rowCount = -1;
    }
}

void Matrix::storeNumberVector(deque<int> number) {
    Matrix::numbers.push_back(number);
}

void Matrix::convertColumnMatrix() {
    deque<deque<int> > newNumbers;

    deque<int> newCols[colCount];
    for (int i = 0; i < numbers.size(); ++i) {
        for (int j = 0; j < numbers.at(i).size(); ++j) {
            int number = numbers.at(i).at(j);
            newCols[j].push_back(number);
        }
    }

    for (int i = 0; i < colCount; i++) {
        newNumbers.push_back(newCols[i]);
    }

    numbers = newNumbers;
}

bool Matrix::controlMatrixSize() {
    int counter = -1;
    int *number, *setNumber;
    if (type == ROW_TYPE) {
        setNumber = &colCount;
        number = &counter;
    } else {
        setNumber = &rowCount;
        int n = (int) numbers.size();
        number = &n;
    }

    for (unsigned long i = 0; i < numbers.size(); ++i) {
        if (counter == -1) {
            counter = (int) numbers.at(i).size();
        } else if (counter != numbers.at(i).size()) {
            return false;
        }

        *setNumber = *number;
    }

    return true;
}

void Matrix::storeNumbersFromFile(const char *fileName) {
    bool first = true;
    bool wasFirst = false;

    bool negative = false;
    deque<int> numberQueue;

    fstream file;
    file.open(fileName);

    string line;
    string number;

    while (getline(file, line)) {
        if (!first) {
            wasFirst = false;
        }

        number.clear();

        for (int i = 0; i < line.length(); i++) {
            char character = line[i];

            if (!isNumber(character) && character != '-' && character != ' ') {
                failed = true;
                return;
            }

            if (first) {
                setSizeCount(character - '0');

                first = false;
                wasFirst = true;

            } else if (wasFirst) {
                failed = true;
                return;

            } else if (isNumber(character)) {
                number += character;

            } else if (character == '-') {
                if (negative || !number.empty()) {
                    failed = true;
                    return;
                }

                negative = true;
                number += character;

            } else if (character == ' ') {
                if (negative && (number.length() == 1)) {
                    failed = true;
                    return;
                }

                numberQueue.push_back(atoi(number.c_str()));

                number.clear();
                negative = false;
            }

            if (i == (line.length() - 1) && !number.empty()) {
                if (negative && (number.size() == 1)) {
                    failed = true;
                    return;
                }

                numberQueue.push_back(atoi(number.c_str()));

                negative = false;
                number.clear();
            }
        }

        if (!numberQueue.empty()) {
            if (negative) {
                failed = true;
                return;
            }

            negative = false;
            storeNumberVector(numberQueue);
            numberQueue.clear();
        }
    }
}

vector<int> Matrix::getNextNumbers() {
    deque<int> number;
    vector<int> vect;

    number = numbers.front();
    numbers.pop_front();

    while (!number.empty()) {
        int temp = number.front();
        vect.push_back(temp);
        number.pop_front();
    }

    return vect;
}

deque<int> Matrix::getNextNumbersDeque() {
    deque<int> number;

    number = numbers.front();
    numbers.pop_front();

    return number;
}

bool Matrix::isFailed() {
    return failed;
}

void Matrix::setSize(int rowCount, int colCount) {
    Matrix::rowCount = rowCount;
    Matrix::colCount = colCount;
}

void Matrix::setEqualSize(int equalSize) {
    Matrix::equalSize = equalSize;
}

int Matrix::getEqualSize() {
    return equalSize;
}