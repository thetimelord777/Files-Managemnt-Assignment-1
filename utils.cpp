#include <cstdlib>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <cstring>
#include <vector>

using namespace std;

struct PrimaryIndexRecord {
  int RRN;
  int byteOffset;
};

struct SecondaryIndexRecord {
  string key;
  vector<int> RRNs;
};

const char DELETE_FLAG = '*';
const char HEADER_PADDING_FLAG = ' ';
const char FIELD_DELIMITER = '|';
const char ACTIVE_FLAG = ' ';
const int EMPLOYEE_RECORD_SIZE = 144;
const int EMPLOYEE_ID_FIELD_SIZE = 13;
const int DEPT_ID_FIELD_SIZE = 30;
const int DEPARTMENT_RECORD_SIZE = 130;
const int DEPT_NAME_FIELD_SIZE = 50;
const int FILE_HEADER_SIZE = sizeof(int);//each file starts with an Avail vector Header.
const int RECORD_HEADER_SIZE = sizeof(int)+1;//each record starts with a fixed length header consisting of Active/Delete flag and size header
const int P_INDEX_RECOD_POINTER_SIZE = 4;
const int S_INDEX_RECOD_POINTER_SIZE = 4;
const int SL_INDEX_RECOD_POINTER_SIZE = 4;
//linked vector to store index records for sorting and binary searches.
//they are read in the inititalise function, modified during data file change operation
//and written back in file when use choose to close the program.
vector<PrimaryIndexRecord> pIndexEmployee,pIndexDepartment;
vector<SecondaryIndexRecord> sIndexEmployee,sIndexDepartment;


//File Structure Examples.

// header 4 bits + 1 bit free for delete/Active flag + 4 bit for record size + fields and delimiters
//if deleted the first field will be overwritten to the next RRN in the Avali vector + field deleimiter
//applies to both employee and department files
//   2   23123|213|Ahmed|Engineer|*  28-1||213|ALI|Engineer|

//Primary and Secondary index files for both employee and department.
//PrimaryIndex Example(Fixed Records Size of 8 bytes:
//   1   5   2  25   3  48   4  71   5  93

//Secondary index is loosely binded to the primary index.
//each record here points to the secondary index list file which
//uses records of fixed sized that point to each other
//SecondaryIndex Example:

//SecondaryIndex list is a data file acting as linked list for secondary index.
//its fixed size (8 bytes each records) and they point to each other.
//SecondaryIndex list Example:

fstream fEmployee("employee.txt", ios::in | ios::out);
fstream fDepartment("depatment.txt", ios::in | ios::out);
fstream fPIndexEmployee("pindexemployee.txt", ios::in | ios::out);
fstream fSIndexEmployee("sindexemployee.txt", ios::in | ios::out);
fstream fSIndexEmployeeData("sindexemployeelist.txt", ios::in | ios::out);

fstream fPIndexDepartment("pindexdepartment.txt", ios::in | ios::out);
fstream fSIndexDepartment("sindexdepartment.txt", ios::in | ios::out);
fstream fSIndexDepartmentData("sindexdepartmentlist.txt", ios::in | ios::out);

//this function can be used to read any field from a file given they are delimiter seperated.
//pass the file,char array and delimiter to it and it will places the read value in the char array.
//example
//file1: ahmed|Doctor|
//file1.seekg(6,ios::begin);
//char job[20];
//readField(file1,job,'|');
void readField(fstream &f,char *key,char delimeter){
    char temp = ' ';
    string value= "";
    temp = f.get();
    while(temp!=delimeter){
        value.push_back(temp);
        temp = f.get();
    }
    strcpy(key,value.c_str());
}

//overwritten function for testing
string readField(fstream &f,char delimeter){
    char temp = ' ';
    string value= "";
    temp = f.get();
    while(temp!=delimeter){
        value.push_back(temp);
        temp = f.get();
    }
    return value;
}

//this function is used to read a fixed size of bytes and returns the result as a string.
//usually used to read Headers of file or records.
//example
//file1: ahmed|Doctor|  23Ali|
//file1.seekg(13,ios::begin);
//string s =readBytes(file1,4);
//s will contain "  23";
string readBytes(fstream &f,int byteCount){
    char temp = ' ';
    string value= "";
    for(int i=0;i<byteCount;i++){
        temp = f.get();
        value.push_back(temp);
    }
    return value;
}

/* TODO: possibly obsolete helping function once we add indexes.
void seekByRRN(fstream &f,int RRN){
    //TODO: create a method to navigate file by byteOffset using indexes later.
    int currentRRN=1,currentRecordSize=-1;
    //reach the required record sequentially
    f.seekg(FILE_HEADER_SIZE,ios::beg);
    while(currentRRN<RRN){
        f.seekg(1,ios::cur);//skip the Active or Deleted flag
        f.seekp(1,ios::cur);
        string s = readBytes(fEmployee,4);
        currentRecordSize = stoi(s);
        f.seekg(currentRecordSize,ios::cur);
        f.seekp(currentRecordSize,ios::cur);
        currentRRN++;
    }
}*/

//This function is used when you want to overWrite/Write a header for a whole file.
//example:
//file1 should have an avali vector header of size 8 letters and value of 234.
//so we can call writeFileHeader(file1,8,234);
void writeFileHeader(fstream& f,int hSize,int header){
    f.seekp(0,ios::beg);
    int actualHeaderSize = to_string(header).length();

    while(actualHeaderSize<hSize){
        f.put(' ');
        actualHeaderSize++;
    }

    f<<header;
}

//This function is used when you want to write a relative sized value in a fixed length field.
void writeFixedField(fstream& f,int fSize,string s){
    int actualSize = s.length();
    int diff = fSize-actualSize;
    int writtenChars = 0;

    for(int i=diff;i>0;i--){
        f.put(' ');
        writtenChars++;
    }
    for(int i=0;writtenChars!=fSize;i++,writtenChars++){
        f.put(s[i]);
    }


}

int getFileSize(fstream& f){
    int current,full;
    current = f.tellg();
    f.seekg(0,ios::end);
    full = f.tellg();
    f.seekg(current,ios::beg);
    return full;
}

//Binary search for primary indexes that returns byteoffSet.
int binarySearch(vector<PrimaryIndexRecord> v, int RRN)
{
    int size = v.size();
    int first = 0;
    int last = size - 1;
    int middle;
    int position = -1;//default state if not found.
    bool found = false;

    while (found == false && first <= last)
    {
        middle = (first + last) / 2;
        if (v[middle].RRN == RRN)     {
            found = true;
            position = v[middle].byteOffset;
        } else if (v[middle].RRN > RRN){
            last = middle - 1;
        } else{
            first = middle + 1;
        }
    }
    return position;
}

//Binary search for secondary indexes that returns RRNs.
vector<int> binarySearch(vector<SecondaryIndexRecord> v, string key)
{
    int size = v.size();
    int first = 0;
    int last = size - 1;
    int middle;
    vector<int> rrns;
    bool found = false;

    while (found == false && first <= last)
    {
        middle = (first + last) / 2;
        if (v[middle].key == key)     {
            found = true;
            rrns = v[middle].RRNs;
        } else if (v[middle].key > key){
            last = middle - 1;
        } else{
            first = middle + 1;
        }
    }
    return rrns;
}

//this function initialise data file and indexes arrays
void initialise(){

    int temp1;
    int temp2;

    //check if files are opened for the first time to add Avail List header.
    fEmployee.seekp(0,ios::end);
    int header = -1;
    if(fEmployee.tellp() <= 0)
    {
        writeFileHeader(fEmployee,FILE_HEADER_SIZE,header);
    }

    fDepartment.seekp(0,ios::end);
    if(fDepartment.tellp()<= 0)
    {
        writeFileHeader(fDepartment,FILE_HEADER_SIZE,header);
    }

    //Read the primary index files into memory.
    fPIndexEmployee.seekg(0,ios::end);
    temp1 = fPIndexEmployee.tellg();
    fPIndexEmployee.seekg(0,ios::beg);
    if(temp1 != 0){
        temp2 =0;
        while(temp2!=temp1){
            PrimaryIndexRecord pir;
            pir.RRN = stoi(readBytes(fPIndexEmployee,EMPLOYEE_ID_FIELD_SIZE));
            pir.byteOffset = stoi(readBytes(fPIndexEmployee,P_INDEX_RECOD_POINTER_SIZE));
            pIndexEmployee.push_back(pir);
            temp2 += (P_INDEX_RECOD_POINTER_SIZE+EMPLOYEE_ID_FIELD_SIZE);
        }
    }

    fPIndexDepartment.seekg(0,ios::end);
    temp1 = fPIndexDepartment.tellg();
    fPIndexDepartment.seekg(0,ios::beg);
    if(temp1 != 0){
        temp2 =0;
        while(temp2!=temp1){
            PrimaryIndexRecord pir;
            pir.RRN = stoi(readBytes(fPIndexDepartment,DEPT_ID_FIELD_SIZE));
            pir.byteOffset = stoi(readBytes(fPIndexDepartment,P_INDEX_RECOD_POINTER_SIZE));
            pIndexDepartment.push_back(pir);
            temp2+= (P_INDEX_RECOD_POINTER_SIZE+DEPT_ID_FIELD_SIZE);
        }
    }

    //Read Secondary Index and Secondary Index list into memory.
    fSIndexEmployee.seekg(0,ios::end);
    temp1 = fSIndexEmployee.tellg();
    fSIndexEmployee.seekg(0,ios::beg);
    fSIndexEmployeeData.seekg(0,ios::beg);

    if(temp1 != 0){
        temp2 =0;
        while(temp2!=temp1){
            SecondaryIndexRecord sir;
            sir.key = readBytes(fSIndexEmployee,DEPT_ID_FIELD_SIZE);
            //Removes white space from the key if the datafile used padding/Fixed Length Record.
            sir.key.erase(remove(sir.key.begin(), sir.key.end(), ' '), sir.key.end());
            int listElem  = stoi(readBytes(fSIndexEmployee,S_INDEX_RECOD_POINTER_SIZE));
            while(listElem!= -1){
                fSIndexEmployeeData.seekg(listElem*(SL_INDEX_RECOD_POINTER_SIZE+EMPLOYEE_ID_FIELD_SIZE),ios::beg);
                listElem = stoi(readBytes(fSIndexEmployeeData,SL_INDEX_RECOD_POINTER_SIZE));
                sir.RRNs.push_back(stoi(readBytes(fSIndexEmployeeData,EMPLOYEE_ID_FIELD_SIZE)));
            };
            sIndexEmployee.push_back(sir);
            temp2+= (S_INDEX_RECOD_POINTER_SIZE+ DEPT_ID_FIELD_SIZE);
        }
    };

    fSIndexDepartment.seekg(0,ios::end);
    temp1 = fSIndexDepartment.tellg();
    fSIndexDepartment.seekg(0,ios::beg);
    fSIndexDepartmentData.seekg(0,ios::beg);
    if(temp1 != 0){
        temp2 =0;
            while(temp2!=temp1){
            SecondaryIndexRecord sir;
            sir.key = readBytes(fSIndexEmployee,DEPT_NAME_FIELD_SIZE);
            sir.key.erase(remove(sir.key.begin(), sir.key.end(), ' '), sir.key.end());
            int listElem  = stoi(readBytes(fSIndexDepartment,S_INDEX_RECOD_POINTER_SIZE));
            while(listElem!= -1){
                fSIndexDepartmentData.seekg(listElem*(SL_INDEX_RECOD_POINTER_SIZE+DEPT_ID_FIELD_SIZE),ios::beg);
                listElem = stoi(readBytes(fSIndexDepartmentData,SL_INDEX_RECOD_POINTER_SIZE));
                sir.RRNs.push_back(stoi(readBytes(fSIndexDepartmentData,DEPT_ID_FIELD_SIZE)));
            };
            sIndexDepartment.push_back(sir);
            temp2+= (S_INDEX_RECOD_POINTER_SIZE+DEPT_NAME_FIELD_SIZE);
        }
    };

};


//Parameter function to sort the Primary indexes vector.
bool pIndexSorterAscending(PrimaryIndexRecord const& lpir, PrimaryIndexRecord const& rpir) {
    return lpir.RRN < rpir.RRN;
};

//Parameter function to sort the Secondary indexes vector.
bool sIndexSorterAscending(SecondaryIndexRecord const& lsir, SecondaryIndexRecord const& rsir) {
    int compare = rsir.key.compare(lsir.key);
    if(compare>0){
        return true;
    } else {
        return false;
    }
};

//this functions closes the files, sorts the indexes and
// write them into files.
void closeFiles(){
    int x,y;

    //sort the vector for primary indexes using byteoffset/ and the sort function above
    //and clears the datafile using trunc and write the sorted data into the file.
    fPIndexEmployee.close();
    fPIndexEmployee.open("pindexemployee.txt",ios::trunc | ios::out);
    sort(pIndexEmployee.begin(), pIndexEmployee.end(), &pIndexSorterAscending);
    //fPIndexEmployee.clear();
    x = pIndexEmployee.size();
    for (int i=0;i<x; i++){
        writeFixedField(fPIndexEmployee,EMPLOYEE_ID_FIELD_SIZE,to_string(pIndexEmployee[i].RRN));
        writeFixedField(fPIndexEmployee,P_INDEX_RECOD_POINTER_SIZE,to_string(pIndexEmployee[i].byteOffset));
    }

    fPIndexDepartment.close();
    fPIndexDepartment.open("pindexdepartment.txt",ios::trunc | ios::out);
    sort(pIndexDepartment.begin(), pIndexDepartment.end(), &pIndexSorterAscending);
    x = pIndexDepartment.size();
    for (int i=0;i<x; i++){
        writeFixedField(fPIndexDepartment,DEPT_ID_FIELD_SIZE,to_string(pIndexDepartment[i].RRN));
        writeFixedField(fPIndexDepartment,P_INDEX_RECOD_POINTER_SIZE,to_string(pIndexDepartment[i].byteOffset));
    }



    //sort the vector for secondary indexes using string compasring and the sort function above
    //and clears the datafile using trunc and write the sorted data into the 2 files.
    fSIndexEmployee.close();
    fSIndexEmployee.open("sindexemployee.txt",ios::trunc | ios::out);
    fSIndexEmployeeData.close();
    fSIndexEmployeeData.open("sindexemployeelist.txt",ios::trunc | ios::out);
    sort(sIndexEmployee.begin(), sIndexEmployee.end(), &sIndexSorterAscending);
    x = sIndexEmployee.size();
    for (int i1=0;i1<x; i1++){
        writeFixedField(fSIndexEmployee,DEPT_ID_FIELD_SIZE,sIndexEmployee[i1].key);
        writeFixedField(fSIndexEmployee,S_INDEX_RECOD_POINTER_SIZE,to_string(fSIndexEmployeeData.tellg()/(SL_INDEX_RECOD_POINTER_SIZE+EMPLOYEE_ID_FIELD_SIZE)));

        y = sIndexEmployee[i1].RRNs.size();
        for (int i2=0;i2<y; i2++){
            if(i2==y-1){
                writeFixedField(fSIndexEmployeeData,SL_INDEX_RECOD_POINTER_SIZE,to_string(-1));
                writeFixedField(fSIndexEmployeeData,EMPLOYEE_ID_FIELD_SIZE,to_string(sIndexEmployee[i1].RRNs[i2]));
            }else{
                writeFixedField(fSIndexEmployeeData,SL_INDEX_RECOD_POINTER_SIZE,to_string(fSIndexEmployeeData.tellg()/(SL_INDEX_RECOD_POINTER_SIZE+EMPLOYEE_ID_FIELD_SIZE)+ 1));
                writeFixedField(fSIndexEmployeeData,EMPLOYEE_ID_FIELD_SIZE,to_string(sIndexEmployee[i1].RRNs[i2]));
            }
        }

    }



    fSIndexDepartment.close();
    fSIndexDepartment.open("sindexdepartment.txt",ios::trunc | ios::out);
    fSIndexDepartmentData.close();
    fSIndexDepartmentData.open("sindexdepartmentlist.txt",ios::trunc | ios::out);
    sort(sIndexDepartment.begin(), sIndexDepartment.end(), &sIndexSorterAscending);
    x = sIndexDepartment.size();
    for (int i1=0;i1<x; i1++){
        writeFixedField(fSIndexDepartment,DEPT_NAME_FIELD_SIZE,sIndexDepartment[i1].key);
        writeFixedField(fSIndexDepartment,S_INDEX_RECOD_POINTER_SIZE,to_string(fSIndexDepartmentData.tellg()/(SL_INDEX_RECOD_POINTER_SIZE+DEPT_ID_FIELD_SIZE)));

        y = sIndexDepartment[i1].RRNs.size();
        for (int i2=0;i2<y; i2++){
            if(i2==y-1){
                writeFixedField(fSIndexDepartmentData,SL_INDEX_RECOD_POINTER_SIZE,to_string(-1));
                writeFixedField(fSIndexDepartmentData,DEPT_ID_FIELD_SIZE,to_string(sIndexDepartment[i1].RRNs[i2]));
            }else{
                writeFixedField(fSIndexDepartmentData,SL_INDEX_RECOD_POINTER_SIZE,to_string(fSIndexDepartmentData.tellg()/(SL_INDEX_RECOD_POINTER_SIZE+DEPT_ID_FIELD_SIZE) + 1));
                writeFixedField(fSIndexDepartmentData,DEPT_ID_FIELD_SIZE,to_string(sIndexDepartment[i1].RRNs[i2]));
            }
        }

    }


    fEmployee.close();
    fDepartment.close();
    fPIndexEmployee.close();
    fSIndexEmployee.close();
    fPIndexDepartment.close();
    fSIndexDepartment.close();
};

//testing function
void showvector(vector<PrimaryIndexRecord> g)
{
    int x = g.size();
    for (int i=0;i<x; i++){
        cout << '\t' << g[i].RRN<<"  :  "<< g[i].byteOffset;
    }
    cout << '\n';
};
