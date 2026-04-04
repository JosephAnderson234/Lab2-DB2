import random
import string
import os
from p1 import FixedRecordMoveLast, FixedRecordFreeList

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