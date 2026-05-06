как писать тесты:

Repo repo - временный репозиторий для теста, в конце теста сам удаляется

repo.init() = pear init <repo_path>
repo.deinit() = pear deinit

repo.connect_main(address) = pear connect --main --listen <address>
repo.connect(main_address, address) = pear connect --gu <main_address> --listen <address>
repo.disconnect() = pear disconnect

repo.write_file(path, content) - создает файл внутри репозитория, path относительно корня repo
repo.read_file(path) - читает файл внутри репозитория
repo.remove_file(path) - удаляет файл внутри репозитория
repo.exists(path) - проверяет файл внутри репозитория

repo.add({paths...}) = pear add <paths...>, можно передавать файлы и папки
repo.add_all() = pear add --all

repo.unstage({paths...}) = pear unstage <paths...>
repo.unstage_all() = pear unstage --all

repo.update() = pear update
repo.push() = pear push
repo.pull({targets...}) = pear pull <targets...>

repo.status() = pear status --json, возвращает Status
repo.ls() = pear ls --json, возвращает Ls

repo.raw({args...}) - запасной вариант для команды, которой еще нет в хелпере

local_address() - свободный локальный адрес для тестовой ноды
wait_network() - маленькая пауза после старта демона/соединения

const Status status = repo.status();

status.staged - вектор staged-файлов, у каждого есть path, operation, object_hash

staged_paths(status) - вектор только путей из status.staged
    EXPECT_EQ(staged_paths(status), (std::vector<std::string>{"a.txt"}));

status.staged[i].path
    EXPECT_EQ(status.staged[0].path, "a.txt");

status.staged[i].operation
    EXPECT_EQ(status.staged[0].operation, "add");

status.staged[i].object_hash
    EXPECT_FALSE(status.staged[0].object_hash.empty());

status.modified - вектор измененных путей
    EXPECT_EQ(status.modified, (std::vector<std::string>{"a.txt"}));

status.modified_after_staging - вектор путей, измененных после staging
    EXPECT_EQ(status.modified_after_staging, (std::vector<std::string>{"a.txt"}));

status.missing - вектор путей, которые есть в базе, но пропали с диска
    EXPECT_EQ(status.missing, (std::vector<std::string>{"a.txt"}));

status.untracked - вектор путей, которые есть на диске, но не добавлены
    EXPECT_EQ(status.untracked, (std::vector<std::string>{"a.txt"}));

const Ls ls = repo.ls();

ls.files - вектор файлов, у каждого есть path, object_hash, version, owner_device_id, owner_address

file_paths(ls) - вектор только путей из ls.files
    EXPECT_EQ(file_paths(ls), (std::vector<std::string>{"a.txt"}));

ls.files[i].path
    EXPECT_EQ(ls.files[0].path, "a.txt");

ls.files[i].object_hash
    EXPECT_FALSE(ls.files[0].object_hash.empty());

ls.files[i].version
    EXPECT_EQ(ls.files[0].version, 1);

ls.files[i].owner_device_id
    EXPECT_EQ(ls.files[0].owner_device_id, 0);

ls.files[i].owner_address
    EXPECT_EQ(ls.files[0].owner_address, "127.0.0.1:5000");