/* THREADSAFE_QUEUE */

// Método push: Añade un elemento a la cola
template<typename T>
void threadsafe_queue<T>::push(T new_value) {
	std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));

	// Creamos un nodo nuevo completamente vacío que será el nuevo final
	std::unique_ptr<node> p(new node);

	{
		std::lock_guard<std::mutex> tail_lock(tail_mutex);
		// Modificamos el antiguo final de la cola
		// y ponemos el nodo al que apunta p como nuevo final
		tail->data = new_data;
		node* const new_tail = p.get();
		tail->next = std::move(p);
		tail = new_tail;
	}

	data_cond_added.notify_one(); // Notificamos de que un elemento ha sido añadido
}

/* FUNCTION_THREAD_POOL */

// Método worker_thread: Realiza el trabajo, pensado para ejecutarse en cada uno de los hilos
template<typename P>
void function_thread_pool<P>::worker_thread() {
	// Se estará ejecutando eternamente hasta que hagamos done = true
	while (!done) {
		P p;
		if (work_queue.try_pop(p)) f(p);
		else std::this_thread::yield();
	}
}
