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


class SlottedPage {
public:
    char data[PAGE_SIZE];

    SlottedPage() {
        memset(data, 0, PAGE_SIZE);
        setNumSlots(0);
        setFreePtr(PAGE_SIZE);
    }

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
        int offset = sizeof(int)*2 + i*sizeof(Slot);
        memcpy(&s, data + offset, sizeof(Slot));
        return s;
    }

    void setSlot(int i, Slot s) {
        int offset = sizeof(int)*2 + i*sizeof(Slot);
        memcpy(data + offset, &s, sizeof(Slot));
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
        if (s.length == -1) throw runtime_error("Registro eliminado");

        return deserialize(data + s.offset);
    }

    void removeRecord(int pos) {
        Slot s = getSlot(pos);
        s.length = -1;
        setSlot(pos, s);
    }
};


class HeapFile {
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
        vector<Record> res;
        ifstream file(filename, ios::binary);

        SlottedPage page;

        while (file.read(page.data, PAGE_SIZE)) {
            int n = page.getNumSlots();
            for (int i = 0; i < n; i++) {
                Slot s = page.getSlot(i);
                if (s.length != -1) {
                    res.push_back(deserialize(page.data + s.offset));
                }
            }
        }

        return res;
    }
};

int main() {
    HeapFile hf("data.dat");

    Record r1 = {"A001", 1, 1500.5, "Obs1"};
    Record r2 = {"A002", 2, 2000.0, "Obs2 larga"};
    Record r3 = {"A003", 3, 1800.0, "Otra observacion"};

    hf.add(r1);
    hf.add(r2);
    hf.add(r3);

    auto records = hf.load();

    for (auto& r : records) {
        cout << r.codigo << " "
             << r.ciclo << " "
             << r.mensualidad << " "
             << r.observaciones << endl;
    }
}