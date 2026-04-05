#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct Record {
    std::string codigo;
    int ciclo;
    float mensualidad;
    std::string observaciones;
};

inline void appendBytes(std::vector<char>& out, const void* src, std::size_t count) {
    const char* bytes = static_cast<const char*>(src);
    out.insert(out.end(), bytes, bytes + count);
}

inline void appendInt(std::vector<char>& out, int value) {
    appendBytes(out, &value, sizeof(int));
}

inline void appendFloat(std::vector<char>& out, float value) {
    appendBytes(out, &value, sizeof(float));
}

inline int readInt(const std::vector<char>& data, std::size_t& cursor) {
    if (cursor + sizeof(int) > data.size()) {
        throw std::runtime_error("Buffer insuficiente al leer int");
    }

    int value = 0;
    std::memcpy(&value, data.data() + cursor, sizeof(int));
    cursor += sizeof(int);
    return value;
}

inline float readFloat(const std::vector<char>& data, std::size_t& cursor) {
    if (cursor + sizeof(float) > data.size()) {
        throw std::runtime_error("Buffer insuficiente al leer float");
    }

    float value = 0.0f;
    std::memcpy(&value, data.data() + cursor, sizeof(float));
    cursor += sizeof(float);
    return value;
}

inline std::vector<char> serialize(const Record& r) {
    std::vector<char> out;

    appendInt(out, static_cast<int>(r.codigo.size()));
    appendBytes(out, r.codigo.data(), r.codigo.size());

    appendInt(out, r.ciclo);
    appendFloat(out, r.mensualidad);

    appendInt(out, static_cast<int>(r.observaciones.size()));
    appendBytes(out, r.observaciones.data(), r.observaciones.size());

    return out;
}

inline Record deserialize(const std::vector<char>& data) {
    std::size_t cursor = 0;
    Record result;

    int codigoSize = readInt(data, cursor);
    if (codigoSize < 0 || cursor + static_cast<std::size_t>(codigoSize) > data.size()) {
        throw std::runtime_error("Tamaño inválido para codigo");
    }
    result.codigo.assign(data.data() + cursor, data.data() + cursor + codigoSize);
    cursor += static_cast<std::size_t>(codigoSize);

    result.ciclo = readInt(data, cursor);
    result.mensualidad = readFloat(data, cursor);

    int obsSize = readInt(data, cursor);
    if (obsSize < 0 || cursor + static_cast<std::size_t>(obsSize) > data.size()) {
        throw std::runtime_error("Tamaño inválido para observaciones");
    }
    result.observaciones.assign(data.data() + cursor, data.data() + cursor + obsSize);

    return result;
}

struct Slot {
    int offset;
    int length;
};

struct PageHeader {
    int num_slots;
    int free_ptr;
};

class VariableRecordFile {
private:
    std::fstream file;
    std::string filename;
    static constexpr int PAGE_SIZE = 4096;
    static constexpr int INVALID_LENGTH = 0;

    int pageCount() {
        file.clear();
        file.seekg(0, std::ios::end);
        const std::streamoff size = file.tellg();
        return static_cast<int>(size / PAGE_SIZE);
    }

    std::vector<char> readPage(int pageId) {
        if (pageId < 0 || pageId >= pageCount()) {
            throw std::out_of_range("Página fuera de rango");
        }

        std::vector<char> page(PAGE_SIZE, 0);
        file.clear();
        file.seekg(static_cast<std::streamoff>(pageId) * PAGE_SIZE, std::ios::beg);
        file.read(page.data(), PAGE_SIZE);

        if (!file) {
            throw std::runtime_error("No se pudo leer la página");
        }

        return page;
    }

    void writePage(int pageId, const std::vector<char>& page) {
        if (static_cast<int>(page.size()) != PAGE_SIZE) {
            throw std::invalid_argument("Página inválida: tamaño incorrecto");
        }

        file.clear();
        file.seekp(static_cast<std::streamoff>(pageId) * PAGE_SIZE, std::ios::beg);
        file.write(page.data(), PAGE_SIZE);

        if (!file) {
            throw std::runtime_error("No se pudo escribir la página");
        }

        file.flush();
    }

    PageHeader readHeader(const std::vector<char>& page) {
        PageHeader header{};
        std::memcpy(&header, page.data(), sizeof(PageHeader));
        return header;
    }

    void writeHeader(std::vector<char>& page, const PageHeader& header) {
        std::memcpy(page.data(), &header, sizeof(PageHeader));
    }

    Slot readSlot(const std::vector<char>& page, int slotId) {
        const std::size_t offset = sizeof(PageHeader) + static_cast<std::size_t>(slotId) * sizeof(Slot);
        Slot slot{};
        std::memcpy(&slot, page.data() + offset, sizeof(Slot));
        return slot;
    }

    void writeSlot(std::vector<char>& page, int slotId, const Slot& slot) {
        const std::size_t offset = sizeof(PageHeader) + static_cast<std::size_t>(slotId) * sizeof(Slot);
        std::memcpy(page.data() + offset, &slot, sizeof(Slot));
    }

    int freeSpace(const PageHeader& header) const {
        const int slotDirectoryEnd =
            static_cast<int>(sizeof(PageHeader) + header.num_slots * static_cast<int>(sizeof(Slot)));
        return header.free_ptr - slotDirectoryEnd;
    }

    void initializeEmptyPage(std::vector<char>& page) const {
        page.assign(PAGE_SIZE, 0);
        PageHeader header{};
        header.num_slots = 0;
        header.free_ptr = PAGE_SIZE;
        std::memcpy(page.data(), &header, sizeof(PageHeader));
    }

public:
    explicit VariableRecordFile(const std::string& filename) : filename(filename) {
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);

        if (!file.is_open()) {
            std::ofstream creator(filename, std::ios::binary);
            creator.close();

            file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
            if (!file.is_open()) {
                throw std::runtime_error("No se pudo abrir/crear el archivo binario");
            }
        }

        if (pageCount() == 0) {
            std::vector<char> page;
            initializeEmptyPage(page);
            writePage(0, page);
        }
    }

    ~VariableRecordFile() {
        if (file.is_open()) {
            file.close();
        }
    }

    void add(const Record& record) {
        std::vector<char> serialized = serialize(record);
        const int needed = static_cast<int>(serialized.size()) + static_cast<int>(sizeof(Slot));

        int targetPageId = pageCount() - 1;
        std::vector<char> page = readPage(targetPageId);
        PageHeader header = readHeader(page);

        if (freeSpace(header) < needed) {
            targetPageId = pageCount();
            initializeEmptyPage(page);
            header = readHeader(page);
        }

        header.free_ptr -= static_cast<int>(serialized.size());
        const int dataOffset = header.free_ptr;
        std::memcpy(page.data() + dataOffset, serialized.data(), serialized.size());

        Slot slot{};
        slot.offset = dataOffset;
        slot.length = static_cast<int>(serialized.size());
        writeSlot(page, header.num_slots, slot);

        header.num_slots += 1;
        writeHeader(page, header);

        writePage(targetPageId, page);
    }

    Record readRecord(int pos) {
        if (pos < 0) {
            throw std::out_of_range("Posición de registro inválida");
        }

        int remaining = pos;
        const int pages = pageCount();

        for (int pageId = 0; pageId < pages; ++pageId) {
            const std::vector<char> page = readPage(pageId);
            const PageHeader header = readHeader(page);

            if (remaining < header.num_slots) {
                const Slot slot = readSlot(page, remaining);
                if (slot.length == INVALID_LENGTH) {
                    throw std::runtime_error("El registro fue eliminado");
                }

                if (slot.offset < 0 || slot.offset + slot.length > PAGE_SIZE) {
                    throw std::runtime_error("Slot corrupto");
                }

                std::vector<char> payload(static_cast<std::size_t>(slot.length));
                std::memcpy(payload.data(), page.data() + slot.offset, static_cast<std::size_t>(slot.length));
                return deserialize(payload);
            }

            remaining -= header.num_slots;
        }

        throw std::out_of_range("Registro fuera de rango");
    }

    void remove(int pos) {
        if (pos < 0) {
            throw std::out_of_range("Posición de registro inválida");
        }

        int remaining = pos;
        const int pages = pageCount();

        for (int pageId = 0; pageId < pages; ++pageId) {
            std::vector<char> page = readPage(pageId);
            const PageHeader header = readHeader(page);

            if (remaining < header.num_slots) {
                Slot slot = readSlot(page, remaining);
                slot.length = INVALID_LENGTH;
                writeSlot(page, remaining, slot);
                writePage(pageId, page);
                return;
            }

            remaining -= header.num_slots;
        }

        throw std::out_of_range("Registro fuera de rango");
    }

    std::vector<Record> load() {
        std::vector<Record> result;
        const int pages = pageCount();

        for (int pageId = 0; pageId < pages; ++pageId) {
            const std::vector<char> page = readPage(pageId);
            const PageHeader header = readHeader(page);

            for (int slotId = 0; slotId < header.num_slots; ++slotId) {
                const Slot slot = readSlot(page, slotId);
                if (slot.length == INVALID_LENGTH) {
                    continue;
                }

                if (slot.offset < 0 || slot.offset + slot.length > PAGE_SIZE) {
                    throw std::runtime_error("Se encontró un slot inválido durante load()");
                }

                std::vector<char> payload(static_cast<std::size_t>(slot.length));
                std::memcpy(payload.data(), page.data() + slot.offset, static_cast<std::size_t>(slot.length));
                result.push_back(deserialize(payload));
            }
        }

        return result;
    }
};

int main() {
    try {
        const int N = 120;
        std::remove("data.bin");
        VariableRecordFile file("data.bin");

        for (int i = 0; i < N; i++) {
            Record r = {
                "COD" + std::to_string(i),
                i % 10,
                1000.0f + static_cast<float>(i) * 10.0f,
                "obs " + std::to_string(i)
            };
            file.add(r);
        }

        std::cout << "[OK] add(): se insertaron " << N << " registros" << std::endl;

        Record sample = file.readRecord(25);
        std::cout << "[OK] readRecord(25): " << sample.codigo
                  << ", ciclo=" << sample.ciclo
                  << ", mensualidad=" << sample.mensualidad
                  <<", observaciones=" << sample.observaciones << std::endl;

        file.remove(10);
        file.remove(11);
        std::cout << "[OK] remove(): se eliminaron posiciones 10 y 11" << std::endl;

        bool removedThrows = false;
        try {
            (void)file.readRecord(10);
        } catch (const std::exception&) {
            removedThrows = true;
        }

        std::vector<Record> all = file.load();
        std::cout << "[OK] load(): total cargados = " << all.size() << std::endl;

        if (!removedThrows) {
            std::cerr << "[FAIL] readRecord() no lanzó excepción para registro eliminado" << std::endl;
            return 1;
        }

        if (all.size() != static_cast<std::size_t>(N - 2)) {
            std::cerr << "[FAIL] load() devolvió " << all.size()
                      << " y se esperaba " << (N - 2) << std::endl;
            return 1;
        }

        std::cout << "Pruebas funcionales OK" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
