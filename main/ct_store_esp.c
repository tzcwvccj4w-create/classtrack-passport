#include "ct_store_esp.h"

#include "esp_partition.h"

static int partition_read(void *context, size_t offset, void *data, size_t length)
{
    return esp_partition_read((const esp_partition_t *)context, offset, data, length) == ESP_OK ? 0 : -1;
}

static int partition_write(void *context, size_t offset, const void *data, size_t length)
{
    return esp_partition_write((const esp_partition_t *)context, offset, data, length) == ESP_OK ? 0 : -1;
}

static int partition_erase(void *context, size_t offset, size_t length)
{
    return esp_partition_erase_range((const esp_partition_t *)context, offset, length) == ESP_OK ? 0 : -1;
}

ct_store_result_t ct_store_init_partition(ct_store_t *store)
{
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, 0x40, "classdata");
    if (!partition) return CT_STORE_ERR_FORMAT;
    ct_store_backend_t backend = {
        .context = (void *)partition,
        .size = partition->size,
        .read = partition_read,
        .write = partition_write,
        .erase = partition_erase,
    };
    return ct_store_init(store, &backend);
}
