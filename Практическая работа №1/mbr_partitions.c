#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

/*
 * Структура MBR (Master Boot Record):
 * - байты 0..445   : загрузочный код
 * - байты 446..509 : таблица разделов (4 записи по 16 байт)
 * - байты 510..511 : сигнатура 0x55AA
 */

#define MBR_SIZE 512
#define PARTITION_TABLE_OFFSET 446
#define PARTITION_COUNT 4
#define PARTITION_ENTRY_SIZE 16

static uint32_t read_le32(const unsigned char *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static const char *fs_type_name(unsigned char type)
{
    switch (type) {
        case 0x00: return "Не используется";
        case 0x01: return "FAT12";
        case 0x04: return "FAT16 (<32MB)";
        case 0x05: return "Расширенный раздел";
        case 0x06: return "FAT16";
        case 0x07: return "NTFS/exFAT/HPFS";
        case 0x0B: return "FAT32";
        case 0x0C: return "FAT32 (LBA)";
        case 0x0E: return "FAT16 (LBA)";
        case 0x0F: return "Расширенный раздел (LBA)";
        case 0x82: return "Linux swap";
        case 0x83: return "Linux";
        case 0x8E: return "Linux LVM";
        case 0xA5: return "FreeBSD";
        case 0xAF: return "macOS HFS/HFS+";
        case 0xEE: return "GPT Protective MBR";
        default: return "Неизвестный тип";
    }
}

int main(int argc, char *argv[])
{
    int fd;
    unsigned char mbr[MBR_SIZE];
    int i;
    int bytes_read;
    int found = 0;

    if (argc != 2) {
        printf("Использование: %s <файл_с_MBR>\n", argv[0]);
        fflush(stdout);
        return 1;
    }

    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("Ошибка: не удалось открыть файл '%s'.\n", argv[1]);
        fflush(stdout);
        return 1;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
        printf("Ошибка: не удалось установить позицию чтения в файле.\n");
        fflush(stdout);
        close(fd);
        return 1;
    }

    bytes_read = read(fd, mbr, MBR_SIZE);
    if (bytes_read != MBR_SIZE) {
        printf("Ошибка: не удалось прочитать первые 512 байт файла.\n");
        fflush(stdout);
        close(fd);
        return 1;
    }

    close(fd);

    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        printf("Ошибка: сигнатура загрузочного сектора 0x55AA не найдена.\n");
        fflush(stdout);
        return 1;
    }

    printf("Сигнатура MBR корректна (0x55AA).\n\n");

    for (i = 0; i < PARTITION_COUNT; i++) {
        const unsigned char *entry = mbr + PARTITION_TABLE_OFFSET + (i * PARTITION_ENTRY_SIZE);
        unsigned char boot_flag = entry[0];
        unsigned char partition_type = entry[4];
        uint32_t start_lba = read_le32(entry + 8);
        uint32_t sector_count = read_le32(entry + 12);

        /* Считаем раздел существующим, если указан тип и размер больше нуля. */
        if (partition_type == 0x00 || sector_count == 0) {
            continue;
        }

        found = 1;

        printf("Раздел %d\n", i + 1);
        printf("Файловая система: %s (код 0x%02X)\n", fs_type_name(partition_type), partition_type);
        printf("Загрузочный: %s\n", (boot_flag == 0x80) ? "да" : "нет");
        printf("Начало (LBA): %u\n", start_lba);
        printf("Размер (секторов): %u\n\n", sector_count);
    }

    if (!found) {
        printf("В таблице разделов нет существующих разделов.\n");
    }

    fflush(stdout);
    return 0;
}
