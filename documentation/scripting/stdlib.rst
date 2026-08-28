zen standard library
=====================

The zen VM ships a small standard library. Kinetix2D registers five modules:
``math``, ``time``, ``json``, ``net`` and ``http``. They are imported the
usual way:

.. code-block:: python

   import math

   speed = math.sqrt(64.0)
   angle = math.degrees(math.pi)

The base library is always open, so its functions need no import. The ``io``,
``os``, ``path`` and ``struct`` modules exist in the VM but are not registered
by Kinetix2D: filesystem access from scripts stays limited on purpose. For
text files inside the user folder, scripts use ``user_data_read_text`` and
``user_data_write_text``; game assets are read through the engine's asset
loading.

Base globals (always available)
-------------------------------

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Function
     - Purpose
   * - ``str(v)``
     - Converts a value to a string.
   * - ``int(v)``
     - Converts to an integer.
   * - ``float(v)``
     - Converts to a float.
   * - ``char(code)``, ``ord(c)``
     - Code point <-> character.
   * - ``typeof(v)``
     - Returns the type of a value.
   * - ``isNil``, ``isBool``, ``isInt``, ``isFloat``, ``isNumber``, ``isString``, ``isArray``, ``isMap``, ``isFunction``
     - Type checks.
   * - ``type(v)``, ``isinstance(v, cls)``
     - Type / class checks.
   * - ``range(...)``
     - Builds a range.
   * - ``enumerate(seq)``, ``zip(...)``, ``map(fn, seq)``, ``filter(fn, seq)``
     - Iteration helpers.
   * - ``format(...)``
     - Formats a string.
   * - ``assert(cond)``, ``error(msg)``, ``input(...)``
     - Assertions, errors and line input.
   * - ``collect()``, ``gc_pause()``, ``gc_resume()``, ``mem_used()``, ``mem_info()``
     - Garbage collector control and memory stats.
   * - ``clock()``
     - A clock value in seconds.
   * - ``Int8Array``, ``Int16Array``, ``Int32Array``, ``Uint8Array``, ``Uint16Array``, ``Uint32Array``, ``Float32Array``, ``Float64Array``
     - Typed array constructors.

The base library also defines the ``Signal`` class: ``Signal()``,
``signal.connect(callback)``, ``signal.disconnect(callback)``,
``signal.emit(args...)``, ``signal.count()`` and ``signal.clear()``. It is a
GDScript-style event object; a ``connect``ed callback is invoked on every
``emit``.

.. note::

   The base library also defines ``open()`` and the ``File`` class. They route
   through host I/O callbacks that Kinetix2D does not install, so they are not
   usable from Kinetix2D scripts. Use the ``user_data_*`` text helpers instead.

``math`` module
---------------

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Function
     - Purpose
   * - ``sin``, ``cos``, ``tan``, ``asin``, ``acos``, ``atan``, ``atan2``
     - Trigonometric functions (radians).
   * - ``sqrt``, ``cbrt``, ``exp``, ``log``, ``log2``, ``log10``, ``pow``
     - Powers and logarithms.
   * - ``ceil``, ``floor``, ``round``, ``abs``, ``min``, ``max``
     - Rounding and extremes.
   * - ``clamp(v, lo, hi)``
     - Clamps a value.
   * - ``lerp(a, b, t)``, ``hermite(a, b, t)``, ``smooth_step(a, b, t)``
     - Interpolation helpers.
   * - ``ping_pong(t, length)``
     - Bounces a value between 0 and ``length``.
   * - ``angle_delta(from, to)``
     - Shortest signed angle in degrees.
   * - ``lerp_angle(current, target, t)``
     - Angle-aware interpolation.
   * - ``random()``, ``random(max)``, ``random(min, max)``
     - Uniform random floats.
   * - ``radians(deg)``, ``degrees(rad)``
     - Unit conversion.
   * - ``hypot(a, b)``
     - Length of a right triangle hypotenuse.
   * - ``isnan(x)``, ``isinf(x)``
     - Float checks.

Constants: ``math.pi``, ``math.e``, ``math.tau``, ``math.inf``, ``math.nan``.

``time`` module
---------------

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Function
     - Purpose
   * - ``time()``
     - Wall clock time in seconds.
   * - ``monotonic()``
     - Monotonic time in seconds (never steps backwards).
   * - ``perf_counter()``
     - High-resolution performance counter.
   * - ``sleep(seconds)``
     - Blocks the script for the given time.

``json`` module
---------------

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Function
     - Purpose
   * - ``json.parse(text)``
     - Parses a JSON string into a value.
   * - ``json.stringify(value, pretty=True)``
     - Serializes a value to a JSON string. The second argument can be a bool
       or an indent width (0–16).

``net`` module
--------------

Sockets. Handles are integers.

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Function
     - Purpose
   * - ``resolve(host)``
     - Resolves a host name to an address.
   * - ``tcp_connect(host, port)``
     - Opens a TCP connection.
   * - ``tcp_listen(port, ...)``
     - Listens on a TCP port.
   * - ``tcp_accept(handle)``
     - Accepts an incoming connection.
   * - ``udp_create(...)``
     - Creates a UDP socket.
   * - ``send(handle, data)``, ``recv(handle, ...)``
     - Send / receive on a connected socket.
   * - ``sendto(handle, data, addr, port)``, ``recvfrom(handle, ...)``
     - Send / receive on an unconnected socket.
   * - ``set_blocking(handle, bool)``, ``set_nodelay(handle, bool)``
     - Socket options.
   * - ``poll(handle, timeout_ms)``
     - Waits for readable data.
   * - ``close(handle)``
     - Closes the socket. Returns a bool.

``http`` module
---------------

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Function
     - Purpose
   * - ``http.get(url, ...)``
     - HTTP GET request.
   * - ``http.post(url, ...)``
     - HTTP POST request.
   * - ``http.download(url, ...)``
     - Downloads a resource.
   * - ``http.ping(url, ...)``
     - Connectivity check.
   * - ``http.get_local_ip()``
     - Returns the local IP address.
