#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

using namespace std;

const int PAGE_SIZE = 512;

struct Record {
    string codigo;
    int ciclo;
    float mensualidad;
    string observaciones;
};

vector<char> serialize(const Record& r) {
    vector<char> buffer;

    auto writeString = [&](const string& s) {
        int len = s.size();
        buffer.insert(buffer.end(), (char*)&len, (char*)&len + sizeof(int));
        buffer.insert(buffer.end(), s.begin(), s.end());
    };

    writeString(r.codigo);

    buffer.insert(buffer.end(), (char*)&r.ciclo, (char*)&r.ciclo + sizeof(int));
    buffer.insert(buffer.end(), (char*)&r.mensualidad, (char*)&r.mensualidad + sizeof(float));

    writeString(r.observaciones);

    return buffer;
}

Record deserialize(const char* data) {
    Record r;
    int pos = 0;

    auto readString = [&](string& s) {
        int len;
        memcpy(&len, data + pos, sizeof(int));
        pos += sizeof(int);

        s.assign(data + pos, len);
        pos += len;
    };

    readString(r.codigo);

    memcpy(&r.ciclo, data + pos, sizeof(int));
    pos += sizeof(int);

    memcpy(&r.mensualidad, data + pos, sizeof(float));
    pos += sizeof(float);

    readString(r.observaciones);

    return r;
}

struct Slot {
    int offset;
    int length;
};

// ======================= SLOTTED PAGE =======================
class SlottedPage {
public:
    char data[PAGE_SIZE];

    SlottedPage() {
        memset(data, 0, PAGE_SIZE);
        setNumSlots(0);
        setFreePtr(PAGE_SIZE);
    }

    // -------- HEADER --------
    int getNumSlots() {
        return *(int*)data;
    }

    void setNumSlots(int n) {
        memcpy(data, &n, sizeof(int));
    }

    int getFreePtr() {
        return *(int*)(data + sizeof(int));
    }

    void setFreePtr(int ptr) {
        memcpy(data + sizeof(int), &ptr, sizeof(int));
    }

    Slot getSlot(int i) {
        Slot s;
        int pos = sizeof(int)*2 + i*sizeof(Slot);
        memcpy(&s, data + pos, sizeof(Slot));
        return s;
    }

    void setSlot(int i, Slot s) {
        int pos = sizeof(int)*2 + i*sizeof(Slot);
        memcpy(data + pos, &s, sizeof(Slot));
    }

    
    bool addRecord(const Record& r) {
        vector<char> bytes = serialize(r);
        int recSize = bytes.size();

        int numSlots = getNumSlots();
        int freePtr = getFreePtr();

        int headerEnd = sizeof(int)*2 + (numSlots+1)*sizeof(Slot);

        if (freePtr - recSize < headerEnd) return false;

        freePtr -= recSize;
        memcpy(data + freePtr, bytes.data(), recSize);

        Slot s = {freePtr, recSize};
        setSlot(numSlots, s);

        setNumSlots(numSlots + 1);
        setFreePtr(freePtr);

        return true;
    }

    Record readRecord(int pos) {
        Slot s = getSlot(pos);

        if (s.length == -1)
            throw runtime_error("Registro eliminado");

        return deserialize(data + s.offset);
    }

    void removeRecord(int pos) {
        Slot s = getSlot(pos);
        s.length = -1; // marcar eliminado
        setSlot(pos, s);
    }
};

// ======================= HEAP FILE =======================
class HeapFile {
private:
    string filename;

public:
    HeapFile(string fname) : filename(fname) {}

    void add(const Record& r) {
        fstream file(filename, ios::in | ios::out | ios::binary);

        if (!file) {
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);
        }

        SlottedPage page;
        bool inserted = false;

        file.seekg(0, ios::beg);

        while (file.read(page.data, PAGE_SIZE)) {
            if (page.addRecord(r)) {
                file.seekp(-PAGE_SIZE, ios::cur);
                file.write(page.data, PAGE_SIZE);
                inserted = true;
                break;
            }
        }

        if (!inserted) {
            SlottedPage newPage;
            newPage.addRecord(r);

            file.clear();
            file.seekp(0, ios::end);
            file.write(newPage.data, PAGE_SIZE);
        }

        file.close();
    }

    vector<Record> load() {
        vector<Record> result;
        ifstream file(filename, ios::binary);

        SlottedPage page;

        while (file.read(page.data, PAGE_SIZE)) {
            int n = page.getNumSlots();

            for (int i = 0; i < n; i++) {
                Slot s = page.getSlot(i);

                if (s.length != -1) {
                    result.push_back(deserialize(page.data + s.offset));
                }
            }
        }

        return result;
    }

    Record readRecord(int globalPos) {
        ifstream file(filename, ios::binary);
        SlottedPage page;

        int count = 0;

        while (file.read(page.data, PAGE_SIZE)) {
            int n = page.getNumSlots();

            for (int i = 0; i < n; i++) {
                Slot s = page.getSlot(i);

                if (s.length != -1) {
                    if (count == globalPos)
                        return deserialize(page.data + s.offset);
                    count++;
                }
            }
        }

        throw runtime_error("Posición fuera de rango");
    }

    void remove(int globalPos) {
        fstream file(filename, ios::in | ios::out | ios::binary);
        SlottedPage page;

        int count = 0;

        while (file.read(page.data, PAGE_SIZE)) {
            int n = page.getNumSlots();

            for (int i = 0; i < n; i++) {
                Slot s = page.getSlot(i);

                if (s.length != -1) {
                    if (count == globalPos) {
                        page.removeRecord(i);

                        file.seekp(-PAGE_SIZE, ios::cur);
                        file.write(page.data, PAGE_SIZE);
                        return;
                    }
                    count++;
                }
            }
        }

        throw runtime_error("Posición no encontrada");
    }
};

int main() {
    HeapFile hf("data.dat");


    for (int i = 0; i < 100; i++) {
        Record r;
        r.codigo = "A" + to_string(1000 + i);
        r.ciclo = (i % 10) + 1;
        r.mensualidad = 1000 + i * 10;
        r.observaciones = "Observacion numero " + to_string(i);

        hf.add(r);
    }

    cout << "Se insertaron 100 registros.\n";

    auto records = hf.load();
    cout << "\nTotal registros leidos: " << records.size() << endl;


    cout << "\nPrimeros 10 registros:\n";
    for (int i = 0; i < 10 && i < records.size(); i++) {
        cout << records[i].codigo << " "
             << records[i].ciclo << " "
             << records[i].mensualidad << " "
             << records[i].observaciones << endl;
    }

    cout << "\nLeyendo posicion 50:\n";
    try {
        Record r = hf.readRecord(50);
        cout << r.codigo << " "
             << r.ciclo << " "
             << r.mensualidad << " "
             << r.observaciones << endl;
    } catch (exception& e) {
        cout << e.what() << endl;
    }


    cout << "\nEliminando posiciones 10, 20, 30...\n";
    hf.remove(10);
    hf.remove(20);
    hf.remove(30);


    records = hf.load();
    cout << "\nTotal registros despues de eliminar: " << records.size() << endl;

    // Mostrar algunos registros para validar
    cout << "\nAlgunos registros despues de eliminar:\n";
    for (int i = 0; i < 10 && i < records.size(); i++) {
        cout << records[i].codigo << " "
             << records[i].ciclo << " "
             << records[i].mensualidad << " "
             << records[i].observaciones << endl;
    }

    return 0;
}