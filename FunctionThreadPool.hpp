#ifndef FUNCTIONTHREADPOOL_HPP_INCLUDED
#define FUNCTIONTHREADPOOL_HPP_INCLUDED

// Bibliotecas que necesitamos para que todo funcione
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <utility>
#include <functional>
#include <condition_variable>
#include <vector>

// Creamos una cola a la cual es segura que accedan varios hilos
template<typename T>
class threadsafe_queue {
private:
	// Este struct simboliza cada uno de los nodos de nuestra cola
	struct node {
		std::shared_ptr<T> data;
		std::unique_ptr<node> next;
	};

	std::mutex head_mutex; // Mutex que usamos para proteger la cabeza de la cola
	std::unique_ptr<node> head; // Cabeza de la cola
	std::mutex tail_mutex; // Mutex que usamos para proteger el final de la cola
	node* tail; // Final de la cola
	
	// condition_variable para cuando se añade un elemento
	std::condition_variable data_cond_added;

	// condition_variable para cuando se elimina un elemento
	std::condition_variable data_cond_popped;

	// Obtenemos el final de la cola
	node* get_tail() {
		std::lock_guard<std::mutex> tail_lock(tail_mutex);
		return tail;
	}

	// Elimina el nodo en la cabeza y devuelve un puntero a él
	// esta función asume que la cabeza ya ha sido protegida
	// y que la operación se ha garantizado que se va a realizar con éxito
	std::unique_ptr<node> pop_head() {
		std::unique_ptr<node> old_head = std::move(head);
		head = std::move(old_head->next);
		data_cond_popped.notify_one(); // Notificamos que un objeto se ha eliminado
		return old_head;
	}

	// Función que espera a que haya datos disponibles
	// y devuelve un lock con el mutex que protege a la cabeza de la cola
	std::unique_lock<std::mutex> wait_for_data() {
		std::unique_lock<std::mutex> head_lock(head_mutex);
		data_cond_added.wait(head_lock, [&]{return head.get() != get_tail();});
		return std::move(head_lock);
	}

	// Espera y elimina la cabeza
	std::unique_ptr<node> wait_pop_head() {
		std::unique_lock<std::mutex> head_lock(wait_for_data());
		return pop_head();
	}

	// Espera y elimina la cabeza
	std::unique_ptr<node> wait_pop_head(T& value) {
		std::unique_lock<std::mutex> head_lock(wait_for_data());
		value = std::move(*head->data);
		return pop_head();
	}

	// Intenta eliminar la cabeza
	std::unique_ptr<node> try_pop_head() {
		std::lock_guard<std::mutex> head_lock(head_mutex);

		if (head.get() == get_tail()) return std::unique_ptr<node>();

		return pop_head();
	}

	// Intenta eliminar la cabeza
	std::unique_ptr<node> try_pop_head(T& value) {
		std::lock_guard<std::mutex> head_lock(head_mutex);

		if (head.get() == get_tail()) return std::unique_ptr<node>();

		value = std::move(*head->data);
		return pop_head();
	}
public:
	// Constructor
	threadsafe_queue() : head(new node), tail(head.get()) {}

	// Esta clase no puede ser CopyConstructible ni CopyAssignable
	threadsafe_queue(const threadsafe_queue& other) = delete;
	threadsafe_queue& operator=(const threadsafe_queue& other) = delete;

	// Usamos el método privado que ya hemos construido para esperar y hacer pop
	std::shared_ptr<T> wait_and_pop() {
		std::unique_ptr<node> const old_head = wait_pop_head();
		return old_head->data;
	}
	
	// Usamos el método privado que ya hemos construido para esperar y hacer pop
	void wait_and_pop(T& value) {
		std::unique_ptr<node> const old_head = wait_pop_head(value);
	}

	// Usamos el método privado que ya hemos creado para intentar hacer pop
	std::shared_ptr<T> try_pop() {
		std::unique_ptr<node> old_head = try_pop_head();
		return (old_head ? old_head->data : std::shared_ptr<T>());
	}
	
	// Usamos el método privado que ya hemos creado para intentar hacer pop
	bool try_pop(T& value) {
		std::unique_ptr<node> const old_head = try_pop_head(value);
		return old_head.get() != nullptr;
	}

	// Añadimos un nuevo elemento a la cola
	// Su implementación está en FunctionThreadPool.tpp
	void push(T new_value);

	// Comprueba si la cola está vacía o no
	bool empty() {
		std::lock_guard<std::mutex> head_lock(head_mutex);
		return (head.get() == get_tail());
	}

	// Espera hasta que la cola se quede vacía
	void wait_for_empty() {
		std::unique_lock<std::mutex> head_lock(head_mutex);
		data_cond_popped.wait(head_lock, [&]{return head.get() == get_tail();});
	}
};

// Clase que solo está formada por una referencia a un vector de hilos
// cuyo único propósito es joinear los hilos en caso de ser destruída
// para que estos puedan acabar su trabajo
class join_threads {
	std::vector<std::thread>& threads; // Referencia a un vector de hilos
public:
	// Constructor
	explicit join_threads(std::vector<std::thread>& threads_) : threads(threads_) {}

	// Destructor
	~join_threads() {
		for (unsigned i = 0; i < threads.size(); i++) {
			if (threads[i].joinable()) threads[i].join();
		}
	}
};

// Finalmente creamos nuestra thread pool
// Es una plantilla que recibe el tipo que queremos usar como parámetro de la función
template<typename P>
class function_thread_pool {
	// Valor booleano que nos sirve para indicar que el proceso acabó
	std::atomic_bool done;

	// Función que vamos a estar ejecutando
	std::function<void(const P&)> f;

	// Cola de trabajo en la que guardaremos
	// los parámetros con los que queremos ejecutar nuestra función 
	threadsafe_queue<P> work_queue;

	// Vector de hilos
	std::vector<std::thread> threads;

	// Joiner
	join_threads joiner;

	// Método que usaremos para ejecutar en cada uno de los hilos
	// y que así vaya haciendo el trabajo
	// Su implementación está en FunctionThreadPool.tpp
	void worker_thread();
public:
	// Constructor
	function_thread_pool(const std::function<void(const P&)>& f_,
			unsigned pool_size = std::thread::hardware_concurrency())
			: done(false), f(f_), joiner(threads) {
		// Si el tamño de la pool es menor que 2 se entiende
		// que hubo algún problema y se hace este 2
		pool_size = (pool_size > 2 ? pool_size : 2);

		// Intentamos lanzar en cada uno de los hilos el método worker_thread
		try {
			for (unsigned i = 0; i < pool_size; i++)
				threads.push_back(std::thread(&function_thread_pool::worker_thread, this));
		} catch(...) {
			// Si durante la creación de los hilos hay alguna excepción
			// se fija done = true para que los hilos acaben
			// y se lanza la excepción
			done = true;
			throw;
		}
	}

	// Al destruirlo se hace done = true para que todos los hilos puedan acabar
	// pese a ello y gracias a joiner los hilos que estén actualmente en trabajo acabarán
	~function_thread_pool() {
		done = true;
	}

	// Mandamos unos nuevos parámetros a la cola de trabajo
	void submit(const P& p) {
		work_queue.push(p);
	}

	// Espera hasta que la cola de tareas esté vacía
	// que dicha cola esté vacía no significa que ya no haya tareas ejecutándose
	// pero gracias al joiner se garantiza que estas tareas van a acabar
	void wait() {
		work_queue.wait_for_empty();
	}
};

#include "FunctionThreadPool.tpp"

#endif // FUNCTIONTHREADPOOL_HPP_INCLUDED
