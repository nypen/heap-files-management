#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "heap_file.h"
#include "bf.h"

HP_ErrorCode HP_Init() {
  return HP_OK;
}

HP_ErrorCode HP_CreateIndex(const char *filename){
    BF_ErrorCode code;
    int file_desc;
    BF_Block *block;
    int x;
    BF_Block_Init(&block);
    if( (code = BF_CreateFile(filename)) == BF_OK ){
        if( (code = BF_OpenFile(filename, &file_desc)) == BF_OK ){
            if( (code = BF_AllocateBlock(file_desc , block )) == BF_OK) {
                char *data = BF_Block_GetData(block);
                char sign[] = "HP$";
                memcpy(data,sign,sizeof(char)*strlen(sign));
                BF_Block_SetDirty(block);
                if((code = BF_UnpinBlock(block))!=BF_OK){
                    BF_PrintError(code);
                    return code;
                }

            }else{
                BF_PrintError(code);
                return code;
            }
        }else{
            BF_PrintError(code);
            return code;
        }
    }else{
        BF_PrintError(code);
        return code;
    }
    BF_Block_Destroy(&block);
    BF_CloseFile(file_desc);
    return HP_OK;
}

HP_ErrorCode HP_OpenFile(const char *fileName, int *fileDesc){
    BF_ErrorCode code;
    BF_Block *block;
    char *data;
    char *sign;
    BF_Block_Init(&block);
    if((code = BF_OpenFile(fileName,fileDesc)) == BF_OK){
        if( (code = BF_GetBlock(*fileDesc,0,block )) == BF_OK ){
            data = BF_Block_GetData(block);
            sign = strtok(data,"$");
            code = BF_UnpinBlock(block);
            if(code != BF_OK){
              BF_PrintError(code);
              return code;
            }
            BF_Block_Destroy(&block);

            if(strcmp(sign,"HP") == 0){
                printf("This is a Heap file!\n");
                return HP_OK;
            }else{
                printf("This is not a heapfile!\n");
                return HP_ERROR;
            }

        }else{
            BF_PrintError(code);
            return code;
        }
    }else{
        BF_PrintError(code);
        return code;
    }
}

HP_ErrorCode HP_CloseFile(int fileDesc) {
    BF_ErrorCode code;
    if( (code = BF_CloseFile(fileDesc)) == BF_OK){
      return BF_OK;
    }else{
        BF_PrintError(code);
        return code;
    }

  return HP_OK;
}

HP_ErrorCode HP_InsertEntry(int fileDesc, Record record){
    BF_ErrorCode code;
    BF_Block *block;
    char *data;
    int counter;
    BF_Block_Init(&block);
    int blocks_num,i;
    int MAX_RECORDS_PER_BLOCK = (BF_BLOCK_SIZE - sizeof(int)) / sizeof(Record);     // max records per block ( - sizeoF(int) ,to count current recs)

    code = BF_GetBlockCounter(fileDesc,&blocks_num);
    if(code != BF_OK){
      BF_PrintError(code);
      return code;
    }
    if(blocks_num == 1){    //the file has only been initialized,first entry
      code = BF_AllocateBlock(fileDesc,block);
      if(code != BF_OK){
        BF_PrintError(code);
        return code;
      }
      counter=0;
    }else{                  //entries have been inserted before
        code = BF_GetBlock(fileDesc,blocks_num-1,block);
        if(code != BF_OK){
          BF_PrintError(code);
          return code;
        }
        data=BF_Block_GetData(block);
        memcpy(&counter,data,sizeof(int));
        if(counter == MAX_RECORDS_PER_BLOCK){     //allocating new block ,setting counter to 0
          code = BF_UnpinBlock(block);
          if(code != BF_OK){
            BF_PrintError(code);
            return code;
          }
          code = BF_AllocateBlock(fileDesc,block);
          if(code != BF_OK){
            BF_PrintError(code);
            return code;
          }
          counter=0;
        }//else counter and block are already set
    }
    data = BF_Block_GetData(block);
    Record tmp;
    memcpy(data+sizeof(int)+sizeof(Record)*counter,&record,sizeof(Record));
    counter++;
    memcpy(data,&counter,sizeof(int));

    BF_Block_SetDirty(block);
    code = BF_UnpinBlock(block);
    BF_Block_Destroy(&block);
    if(code != BF_OK){
      BF_PrintError(code);
      return code;
    }
}

HP_ErrorCode HP_PrintAllEntries(int fileDesc) {
    BF_ErrorCode code;
    int i,blocks_num,size_of_record;
    BF_Block *block;
    BF_Block_Init(&block);
    char *data;
    int j,counter;
    Record record;

    if((code = BF_GetBlockCounter(fileDesc,&blocks_num)) == HP_OK){
        if(blocks_num==1){
          printf("There are no entries\n");
          return HP_OK;
        }
        if((code = BF_GetBlock(fileDesc,0,block)) ==BF_OK){  //read metadata->the size of the records
              data=BF_Block_GetData(block);
              i=0;
              while(data[i]!='$'){
                i++;
              }
              size_of_record = 60;
              BF_UnpinBlock(block);
        }else{
          BF_PrintError(code);
          return code;
        }
        for(i=1;i<blocks_num;i++){  //for every block
            if((code = BF_GetBlock(fileDesc,i,block)) !=BF_OK){
                BF_PrintError(code);
                return(code);
            }
            data = BF_Block_GetData(block);
            memcpy(&counter,data,sizeof(int));
            data += sizeof(int);
            for(j=0;j<counter;j++){
                memcpy(&record,data,sizeof(Record));
                printf("%d,",record.id);
                printf("%s,",record.name);
                printf("%s,",record.surname);
                printf("%s\n",record.city);
                data = data+sizeof(Record);
            }
            if((code = BF_UnpinBlock(block)) != BF_OK){
              BF_PrintError(code);
              return code;
            }
        }
    }else{
      BF_PrintError(code);
      return code;
    }
    return HP_OK;
}

HP_ErrorCode HP_GetEntry(int fileDesc, int rowId, Record *record) {
    int MAX_RECORDS_PER_BLOCK = (BF_BLOCK_SIZE - sizeof(int))/sizeof(Record);
    BF_ErrorCode code;
    int blocks_num,no_block,pos;
    BF_Block *block;
    char *data;
    BF_Block_Init(&block);

    code = BF_GetBlockCounter(fileDesc,&blocks_num);
    if ( code != BF_OK ){
      BF_PrintError(code);
      return code;
    }
    rowId--;
    no_block = rowId / MAX_RECORDS_PER_BLOCK +1;
    code = BF_GetBlock(fileDesc,no_block,block);
    if ( code != BF_OK ){
      BF_PrintError(code);
      return code;
    }
    data = BF_Block_GetData(block);
    pos = rowId%MAX_RECORDS_PER_BLOCK;
    memcpy(record,data+sizeof(int)+sizeof(Record)*pos,sizeof(Record));
    BF_UnpinBlock(block);
    BF_Block_Destroy(&block);

    return HP_OK;
}
