import struct
import os
import random
import string

class FixedRecordMoveLast:
    def __init__(self, filename):
        self.filename = filename
        self.format = "5s11s20s15sid" #5 bytes para codigo, 11 bytes para nombre, 20 bytes para apellidos, 
                                    #15 bytes para carrera, 4 bytes para ciclo (int), 8 bytes para mensualidad (double)
        self.record_size = struct.calcsize(self.format) # 63 bytes por registro
    
    def escribirRegistro(self, record):
        with open(self.filename, "ab") as file:
            packed = struct.pack(self.format, record['codigo'].encode(), record['nombre'].encode(), record['apellidos'].encode()
                                    , record['carrera'].encode(), record['ciclo'], record['mensualidad'])
            file.write(packed)

    def _unpack(self, data):
        res = struct.unpack(self.format, data)
        return {
            "codigo": res[0].decode(),
            "nombre": res[1].decode(),
            "apellidos": res[2].decode(),
            "carrera": res[3].decode(),
            "ciclo": res[4],
            "mensualidad": res[5]
        }

    def leerRegistro(self, index): 
        with open(self.filename, "rb") as file:
            file.seek(index * self.record_size)
            data = file.read(self.record_size)
            if not data:
                return None
            return self._unpack(data)
    
    def load(self):
        records = []
        if not os.path.exists(self.filename):
            return records
            
        with open(self.filename, "rb") as file:
            while True:
                bytes_data = file.read(self.record_size)
                if not bytes_data: # Fin del archivo
                    break
                records.append(self._unpack(bytes_data))
        return records

    def add(self, record):
        with open(self.filename, "ab") as file:
            packed = struct.pack(self.format, record['codigo'].encode(), record['nombre'].encode(), record['apellidos'].encode()
                                    , record['carrera'].encode(), record['ciclo'], record['mensualidad'])
            file.write(packed)

    def readRecord(self, pos):
        offset = pos * self.record_size # posicionamiento del registro
        if offset >= os.path.getsize(self.filename):
            return None
        with open(self.filename, "rb") as file:
            file.seek(offset) # mueve el "puntero" dentro del archivo a una posición específica
            data = file.read(self.record_size)
            if not data:
                return None
            return self._unpack(data)

    def remove(self, pos):
        file_size = os.path.getsize(self.filename)
        total_records = file_size // self.record_size
        if pos >= total_records: return
        
        # Leer el último registro
        last_record_pos = (total_records - 1) * self.record_size
        with open(self.filename, "rb+") as file:
            file.seek(last_record_pos)
            last_data = file.read(self.record_size)
            
            # Sobrescribir el registro en 'pos' con los datos del último
            file.seek(pos * self.record_size) # posicionamiento del registro
            file.write(last_data) 
            
            # Truncar el archivo para eliminar el duplicado al final
            file.truncate(last_record_pos)

class FixedRecordFreeList:
    def __init__(self, filename):
        self.filename = filename
        # ? (booleano) + Datos del alumno
        self.record_format = "? 5s 11s 20s 15s i d"
        self.record_size = struct.calcsize(self.record_format)
        # Header: total, activos, free_ptr
        self.header_format = "iii" 
        self.header_size = struct.calcsize(self.header_format)
        self._init_file()

    def _init_file(self):
        if not os.path.exists(self.filename):
            with open(self.filename, "wb") as f:
                # Header inicial: 0 total, 0 activos, -1 free_ptr
                f.write(struct.pack(self.header_format, 0, 0, -1))

    def _pack(self, record, active=True):
        return struct.pack(self.record_format, 
                        active,
                        record['codigo'].encode(),
                        record['nombre'].encode(),
                        record['apellidos'].encode(),
                        record['carrera'].encode(),
                        record['ciclo'],
                        record['mensualidad'])

    def _unpack(self, data):
        res = struct.unpack(self.record_format, data)
        if not res[0]:  # active = False
            return {"active": False}
        return {
            "active": res[0],
            "codigo": res[1].decode(),
            "nombre": res[2].decode(),
            "apellidos": res[3].decode(),
            "carrera": res[4].decode(),
            "ciclo": res[5],
            "mensualidad": res[6]
        }

    def add(self, record):
        with open(self.filename, "rb+") as file:
            file.seek(0)
            total, activos, free_ptr = struct.unpack(self.header_format, file.read(self.header_size))
            
            if free_ptr == -1:
                # No hay huecos, insertar al final
                file.seek(0, os.SEEK_END)
                file.write(self._pack(record, True))
                new_total = total + 1
                new_free = -1
            else:
                # Reutilizar espacio del free_ptr
                file.seek(self.header_size + (free_ptr * self.record_size))
                # El registro borrado guarda el índice al SIGUIENTE hueco
                data_hueco = file.read(self.record_size)
                # El siguiente puntero está justo después del flag en el registro borrado
                next_free = struct.unpack("i", data_hueco[1:5])[0]
                
                file.seek(self.header_size + (free_ptr * self.record_size))
                file.write(self._pack(record, True))
                new_total = total
                new_free = next_free

            # Actualizar Header
            file.seek(0)
            file.write(struct.pack(self.header_format, new_total, activos + 1, new_free))

    def readRecord(self, pos):
        offset = self.header_size + (pos * self.record_size)
        if offset >= os.path.getsize(self.filename):
            return None
            
        with open(self.filename, "rb") as file:
            file.seek(offset)
            data = file.read(self.record_size)
            record = self._unpack(data)
            return record if record["active"] else None

    def remove(self, pos):
        with open(self.filename, "rb+") as file:
            file.seek(0)
            total, activos, free_ptr = struct.unpack(self.header_format, file.read(self.header_size))
            
            # Validar si ya está borrado
            record = self.readRecord(pos)
            if not record: return
            
            # Marcar como inactivo (False) y guardar el free_ptr actual como "siguiente"
            file.seek(self.header_size + (pos * self.record_size))
            # Escribimos: Flag=False (1 byte) + Siguiente puntero (4 bytes)
            file.write(struct.pack("?i", False, free_ptr))
            
            # Actualizar Header: el nuevo free_ptr es esta posición
            file.seek(0)
            file.write(struct.pack(self.header_format, total, activos - 1, pos))

    def load(self):
        records = []
        with open(self.filename, "rb") as file:
            file.seek(self.header_size)
            while True:
                bytes_data = file.read(self.record_size)
                if not bytes_data: break
                record = self._unpack(bytes_data)
                if record["active"]:
                    records.append(record)
        return records

BASE_DIR = os.path.dirname(__file__)

def generate_random_record():
    nombres = [
        'Mateo', 'Sofía', 'Valentina', 'Camila', 'Daniel', 'Alejandro',
        'Lucía', 'Isabella', 'Diego', 'Emilia'
    ]
    apellidos = [
        'González', 'Rodríguez', 'Pérez', 'López', 'Martínez', 'Sánchez',
        'Ramírez', 'Torres', 'Flores', 'Vargas'
    ]
    carreras = [
        'Ingeniería de Sistemas', 'Administración', 'Arquitectura',
        'Ingeniería Civil', 'Marketing', 'Contabilidad', 'Derecho',
        'Medicina', 'Psicología', 'Economía'
    ]
    return {
        'codigo': ''.join(random.choices(string.ascii_uppercase + string.digits, k=5)),
        'nombre': random.choice(nombres),
        'apellidos': f"{random.choice(apellidos)} {random.choice(apellidos)}",
        'carrera': random.choice(carreras),
        'ciclo': random.randint(1, 10),
        'mensualidad': round(random.uniform(100.0, 1000.0), 2)
    }

def test_fixed_record_move_last():
    print("=== Pruebas FixedRecordMoveLast ===")
    filename = os.path.join(BASE_DIR, "test_move_last.dat")
    if os.path.exists(filename):
        os.remove(filename)
    db = FixedRecordMoveLast(filename)

    # Generar 100 registros
    records = [generate_random_record() for _ in range(100)]
    print(f"Generados {len(records)} registros.")

    # Probar escribirRegistro
    print("Escribiendo registros...")
    for rec in records:
        db.escribirRegistro(rec)

    # Probar leerRegistro
    print("Leyendo registros aleatorios...")
    for i in random.sample(range(100), 5):
        result = db.leerRegistro(i)
        if result:
            print(f"Registro {i}: {db.readRecord(i)}")
        else:
            print(f"Registro {i}: None")

    # Probar load()
    print("Cargando todos los registros...")
    all_records = db.load()
    print(f"Total cargados: {len(all_records)}")

    # Probar add()
    print("Agregando un registro extra...")
    extra_rec = generate_random_record()
    db.add(extra_rec)
    print("Registro agregado.")
    all_records = db.load()
    print(f"Total activos después de add(): {len(all_records)}")

    # Probar readRecord()
    print("Probando readRecord en posiciones válidas e inválidas...")
    print(f"readRecord(50): {db.readRecord(50) is not None}")
    print(f"readRecord(150): {db.readRecord(150)}")  # Debería ser None

    # Probar remove()
    print("Eliminando registro en pos 10...")
    print(f"El ultimo registro antes de mover el ultimo registro a la posición del registro eliminado: {db.readRecord(len(all_records) - 1)}")
    db.remove(10)
    print(f"Después de remove, readRecord(10): {db.readRecord(10)}")  # Debería ser el último movido
    print(f"El ultimo registro después de remove: {db.readRecord(len(all_records) - 1)}")
    print("Pruebas FixedRecordMoveLast completadas.\n")

def test_fixed_record_free_list():
    print("=== Pruebas FixedRecordFreeList ===")
    filename = os.path.join(BASE_DIR, "test_free_list.dat")
    if os.path.exists(filename):
        os.remove(filename)
    db = FixedRecordFreeList(filename)

    # Generar 100 registros
    records = [generate_random_record() for _ in range(100)]
    print(f"Generados {len(records)} registros.")

    # Probar add
    print("Agregando registros...")
    for rec in records:
        db.add(rec)

    # Probar readRecord
    print("Leyendo registros aleatorios...")
    for i in random.sample(range(100), 5):
        result = db.readRecord(i)
        if result:
            print(f"Registro {i}: {result}")
        else:
            print(f"Registro {i}: Eliminado")

    # Probar load
    print("Cargando registros activos...")
    active_records = db.load()
    print(f"Total activos: {len(active_records)}")

    # Probar remove
    print("Eliminando registros en pos 5, 15, 25...")
    db.remove(5)
    db.remove(15)
    db.remove(25)
    print(f"Después de remove, readRecord(5): {db.readRecord(5)}")
    print(f"readRecord(15): {db.readRecord(15)}")
    print(f"readRecord(25): {db.readRecord(25)}")
    active_records = db.load()
    print(f"Total activos después de remove: {len(active_records)}")

    # Probar reutilización: agregar después de eliminar
    print("Agregando nuevos registros para reutilizar huecos...")
    db.add(generate_random_record())
    print("Registro agregado. Verificando reutilización...")
    active_after = db.load()
    print(f"Total activos después: {len(active_after)}")

    print("Pruebas FixedRecordFreeList completadas.\n")

if __name__ == "__main__":
    test_fixed_record_move_last()
    test_fixed_record_free_list()
    print("Todas las pruebas completadas.")