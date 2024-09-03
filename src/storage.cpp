// sd card: https://github.com/wemos/D1_mini_Examples/blob/master/examples/04.Shields/Micro_SD_Shield/ReadWrite/ReadWrite.ino

// SD.h SD;
//  -> SDFS.method();
// SDFS.cpp (SDFSConfig : fs::FSConfig); SDFSImpl : fs::FSImpl; (SDFSFileImpl : fs::FileImpl; SDFSDirImpl : fs::DirImpl);
//  -> SDFat _fs, (SDFSConfig _cfg; bool _mounted);
// SDFat.h (SdBase : Vol; SdFat32 : SdBase<FatVolume>; SdExtFat : SdBase<ExFatVolume>; SdFs<FsVolume>); 
//         dFile : PrintFile<SdBaseFile>;
//  -> typedef FatFile SdBaseFile;
// FatLib/FatFile.cpp (FatPos_t; FatLfn_t : public FsName; FatSfn_t); FatFile; (File32 : StreamFile<FatFile, uint32_t>);
// SDFat.h http://pef.if.ufrj.br/producao_academica/artigos/audiotermometro/audiotermometro-I/bibliotecas/SdFat/Doc/html/
//  -> SdCard* m_card; SdCardFactory m_cardFactory;
// SdCard/SdCard.h SdCardFactory
//  -> #include "SdSpiCard"
// SdCard/SdSpiCard.cpp
//  -> #include "SdSpiDriver.h"
// SpiDriver/SdSpiDriver.h

#include "storage.h"
#include <SD.h>
#include <EEPROM.h>

using namespace std;

RandName numToName(const uint8_t num) {
    return { (char)(0x40 | (num >> 4)), (char)(0x40 | (num & 0x0f)), 0x00 };
}
//uint8_t nameToNum(const RandName name) {
//    return (name.bytes[0] << 4) | (name.bytes[1] & 0x0f);
//}

tuple<bool, uint8_t> randFileCount() {
    File root = SD.open("/");
    if (!root) return tuple(false, 0);
    uint8_t count = 0;

    while (true) {
        yield();
        File file = root.openNextFile();
        if (!file) break;
        file.close();
        count += 1;
    }

    root.close();
    if (count < 2) return tuple(false, 0);
    return tuple(true, count - 2);
}

RandFilesResult removeFiles(const uint8_t first_inclusive, const uint8_t last_exclusive) {
    for (uint8_t file_index = first_inclusive; file_index < last_exclusive; file_index += 1) {
        yield();
        if (!SD.remove(numToName(file_index).bytes)) return RandFilesResult::RemoveFailed;
    }
    return RandFilesResult::Ok;
}

//RandFilesResult shiftFiles(const uint8_t rand_file_count) {
//    RandFilesResult result = RandFilesResult::Ok;
//    if (!SD.remove("@@")) result = RandFilesResult::RemoveFailed;
//    for (uint8_t file_index = 0; file_index < rand_file_count - 1; file_index += 1) {
//        if (!SD.rename(numToName(file_index + 1).bytes, numToName(file_index).bytes) && result == RandFilesResult::Ok)
//            result = RandFilesResult::RenameFailed;
//    }
//    return result;
//}
//RandFilesResult rotateFiles(const uint8_t rand_file_count) {
//    RandFilesResult result = RandFilesResult::Ok;
//    if (!SD.rename("@@", numToName(rand_file_count).bytes)) result = RandFilesResult::RenameFailed;
//    for (uint8_t file_index = 0; file_index < rand_file_count; file_index += 1) {
//        if (!SD.rename(numToName(file_index + 1).bytes, numToName(file_index).bytes)) result = RandFilesResult::RenameFailed;
//    }
//    return result;
//}
RandFilesResult shiftFiles(const uint8_t rand_file_count, const uint8_t n) {
    if (!n) return RandFilesResult::Ok;

    bool remove_failed = false;
    for (uint8_t i = 0; i < n; i += 1) {
        yield();
        if (!SD.remove(numToName(i).bytes)) remove_failed = true;
    }
    bool rename_failed = false;
    for (uint8_t i = n; i < rand_file_count; i += 1) {
        yield();
        if (!SD.rename(numToName(i).bytes, numToName(i - n).bytes)) rename_failed = true;
    }
    return remove_failed ? RandFilesResult::RemoveFailed : rename_failed ? RandFilesResult::RenameFailed : RandFilesResult::Ok;
}

void writeError(uint8_t *const error_count, const FullResult error) {
    if (*error_count < ERROR_BUFFER_SIZE) EEPROM.write((*error_count)++, error.value);
}
void writeError(FullResult *const errors, uint8_t *const error_count, const FullResult error) {
    if (*error_count < ERROR_BUFFER_SIZE) errors[(*error_count)++] = error;
}
void writeError(uint8_t *const error_count, const Type type, const Result result) {
    writeError(error_count, FullResult(type, result));
}
void writeError(FullResult *const errors, uint8_t *const error_count, const Type type, const Result error) {
    writeError(errors, error_count, FullResult(type, error));
}
