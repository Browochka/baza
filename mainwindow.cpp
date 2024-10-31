#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // юзеры
    userdb=QSqlDatabase::addDatabase("QSQLITE","userConnection");
    userdb.setDatabaseName("./userdb.db");
    if (userdb.open()) {
        qDebug("userdb was opened");
    }

    query= new QSqlQuery(userdb);
    query->exec("CREATE TABLE Users(Id INTEGER PRIMARY KEY AUTOINCREMENT, Name TEXT, login TEXT, password TEXT, Chats TEXT);");

    tableuser= new QSqlTableModel(this,userdb);
    tableuser->setTable("Users");
    tableuser->insertRow(tableuser->rowCount());
    tableuser->select();

    ui->tableView->setModel(tableuser);

    // чаты
    chatsdb=QSqlDatabase::addDatabase("QSQLITE","chatConnection");
    chatsdb.setDatabaseName("./chatsdb.db");
    if (chatsdb.open()) {
        qDebug("chats was opened");
    }

    querychat= new QSqlQuery(chatsdb);
    querychat->exec("CREATE TABLE Chats(ChatId INTEGER PRIMARY KEY AUTOINCREMENT,members TEXT, Messages TEXT);");

    tablechats= new QSqlTableModel(this,chatsdb);
    tablechats->setTable("Chats");
    tablechats->insertRow(tablechats->rowCount());
    tablechats->select();


    ui->tableView_2->setModel(tablechats);

    // Сообщения
    messagesdb=QSqlDatabase::addDatabase("QSQLITE","messageConnection");
    messagesdb.setDatabaseName("./messagesdb.db");
    if (messagesdb.open()) {
        qDebug("messages was opened");
    }

    querymessages= new QSqlQuery(messagesdb);
    if (!querymessages->exec("CREATE TABLE IF NOT EXISTS Messages(MesId INTEGER PRIMARY KEY AUTOINCREMENT, time TEXT, sender TEXT, ToChat TEXT,writed TEXT);")) {
        qDebug() << "Ошибка выполнения запроса";
    }
    tablemessages= new QSqlTableModel(this,messagesdb);
    tablemessages->setTable("Messages");
    tablemessages->insertRow(tablemessages->rowCount());
    tablemessages->select();


    ui->tableView_3->setModel(tablemessages);


}
bool MainWindow::addUser(const QString& name, const QString& login, const QString& pas) {
    QSqlQuery query(userdb);
    if (userExists(login)){
        qDebug() << "Такой юзер уже существует";
        return false;
    }
    // Шаг 2: Подготавливаем SQL-запрос для вставки данных
    query.prepare("INSERT INTO Users ( Name, login, password,Chats) VALUES ( :name, :login, :password, :chats)");
    //query.bindValue(":id", userid);
    query.bindValue(":name", name);
    query.bindValue(":login", login);
    query.bindValue(":password", pas);
    query.bindValue(":chats","[]");
    // Шаг 3: Выполняем запрос
    if (!query.exec()) {
        qDebug() << "Ошибка выполнения INSERT запроса";
        return false;
    }

    qDebug() << "Запись добавлена успешно!";
    return true;
}
bool MainWindow::userExists(const QString& login) {
    // Подготовка SQL-запроса для поиска пользователя по логину
    QSqlQuery query(userdb);
    query.prepare("SELECT COUNT(*) FROM Users WHERE login = :login");
    query.bindValue(":login", login);

    // Выполняем запрос
    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса на проверку существования пользователя:";
        return false;
    }

    // Проверяем результат
    if (query.next()) {
        int count = query.value(0).toInt();
        return count > 0;  // Если количество > 0, значит пользователь с таким логином уже существует
    }

    return false;
}
bool MainWindow::addChat(int user1_id, int user2_id){
    QSqlQuery querydb(userdb);
    QSqlQuery querychat(chatsdb);
    if (isChatExists(user1_id,user2_id)) {
        qDebug("такой чат уже есть");
        return false;
    }
    QJsonArray membersArray;
    membersArray.append(user1_id);
    membersArray.append(user2_id);

    // Преобразуем массив участников в JSON строку
    QString membersJson = QString(QJsonDocument(membersArray).toJson(QJsonDocument::Compact));

    // Шаг 2: Добавляем новый чат в таблицу Chats
    querychat.prepare("INSERT INTO Chats (members, Messages) VALUES (:members, :Messages)");
    querychat.bindValue(":members", membersJson);
    //querychat.bindValue(":chatid",chatid);
    querychat.bindValue(":Messages","[]");
    if (!querychat.exec()) {
        qDebug() << "Failed to insert chat";
        return false;
    }
    int chat_id =querychat.lastInsertId().toInt();; // Получаем ID последней вставленной записи
    auto addChatIdToUser = [&](int user_id) -> bool {
        querydb.prepare("SELECT Chats FROM Users WHERE Id = :id");
        querydb.bindValue(":id", user_id);

        if (!querydb.exec() || !querydb.next()) {
            qDebug() << "Failed to retrieve user data for user_id:" << user_id;
            return false;
        }

        QString chatsJsonStr = querydb.value("Chats").toString();
        QJsonArray chatsArray;

        // Преобразуем строку JSON в массив
        if (!chatsJsonStr.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(chatsJsonStr.toUtf8());
            chatsArray = doc.array();
        }

        // Проверяем, если chat_id уже существует, чтобы избежать дубликатов
        if (!chatsArray.contains(chat_id)) {
            chatsArray.append(chat_id);
            QString updatedChatsJson = QString(QJsonDocument(chatsArray).toJson(QJsonDocument::Compact));

            // Обновляем поле Chats в базе данных
            querydb.prepare("UPDATE Users SET Chats = :chats WHERE Id = :id");
            querydb.bindValue(":chats", updatedChatsJson);
            querydb.bindValue(":id", user_id);

            if (!querydb.exec()) {
                qDebug() << "Failed to update chat list for user_id:" << user_id;
                return false;
            }
        }
        return true;
    };

    // Обновляем списки чатов для обоих пользователей
    if (!addChatIdToUser(user1_id) || !addChatIdToUser(user2_id)) {
        qDebug() << "Failed to add chat to one or both users.";
        return false;
    }

    return true;
}
bool MainWindow::isChatExists(int user1_id, int user2_id) {
        QSqlQuery querychat(chatsdb);
        QJsonArray members;
        members.append(user1_id);
        members.append(user2_id);
        QJsonArray reversedmembers;
        reversedmembers.append(user2_id);
        reversedmembers.append(user1_id);
        QString membersJson = QString(QJsonDocument(members).toJson(QJsonDocument::Compact));
        QString reversedMembersJson = QString(QJsonDocument(reversedmembers).toJson(QJsonDocument::Compact));

        QString sql = QString("SELECT COUNT(*) FROM Chats WHERE members = :members OR members = :reversedmembers");
        querychat.prepare(sql);
        querychat.bindValue(":members",membersJson);
        querychat.bindValue(":reversedmembers",reversedMembersJson);
        if (!querychat.exec()) {
            qDebug() << "Ошибка выполнения запроса";
            return false;
        }
        if (querychat.next()) {
            int count = querychat.value(0).toInt();
            return count > 0;
        }
        return false;
    }
bool MainWindow::addMessage(const QString& time,int sender,int tochat,const QString& text) {
    QSqlQuery querymes(messagesdb);
    querymes.prepare("INSERT INTO Messages (time, sender,ToChat,writed) VALUES (:time, :sender,:tochat,:text)");
    querymes.bindValue(":time", time);
    querymes.bindValue(":sender", sender);
    querymes.bindValue(":tochat", tochat);
    querymes.bindValue(":text", text);
    if (!querymes.exec()) {
        qDebug() << "Ошибка выполнения INSERT запроса";
        return false;
    }

    qDebug() << "Запись добавлена успешно!";
    int messageid=querymes.lastInsertId().toInt();
    QSqlQuery querychat(chatsdb);
    querychat.prepare("SELECT Messages FROM Chats WHERE ChatId = :chatid");
    querychat.bindValue(":chatid", tochat);

    if (!querychat.exec() || !querychat.next()) {
        qDebug() << "Ошибка получения массива сообщений для чата" ;
        return false;
    }

    // Шаг 3: Парсинг существующего JSON массива и добавление нового messageid
    QString messagesJsonStr = querychat.value("Messages").toString();
    QJsonArray messagesArray;

    // Если массив сообщений не пустой, парсим его
    if (!messagesJsonStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(messagesJsonStr.toUtf8());
        messagesArray = doc.array();
    }

    messagesArray.append(messageid); // Добавляем новый идентификатор сообщения
    QString updatedMessagesJson = QString(QJsonDocument(messagesArray).toJson(QJsonDocument::Compact));

    // Шаг 4: Обновляем поле Messages в таблице Chats
    querychat.prepare("UPDATE Chats SET Messages = :messages WHERE ChatId = :chatid");
    querychat.bindValue(":messages", updatedMessagesJson);
    querychat.bindValue(":chatid", tochat);

    if (!querychat.exec()) {
        qDebug() << "Ошибка обновления массива сообщений в чате";
        return false;
    }

    return true;
}
QList<QMap<QString, QVariant>> MainWindow::autorization(const QString& login, const QString& password) {
    QSqlQuery query(userdb);
    QList<QMap<QString, QVariant>> usersList; // Список для хранения пользователей

    // Выполняем запрос с фильтрацией по login и password
    query.prepare("SELECT Id, Name, Chats FROM Users WHERE login = :login AND password = :password");
    query.bindValue(":login", login);
    query.bindValue(":password", password);

    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса";
        return usersList; // Возвращаем пустой список в случае ошибки
    }


    while (query.next()) {
        QMap<QString, QVariant> user;
        user["id"] = query.value("Id").toInt();
        user["name"] = query.value("Name").toString();


        QString chatsJson = query.value("Chats").toString();
        QJsonDocument chatsDoc = QJsonDocument::fromJson(chatsJson.toUtf8());

        if (chatsDoc.isArray()) {
            user["chats"] = chatsDoc.array().toVariantList();
        } else {
            user["chats"] = QVariantList();
        }

        usersList.append(user);
    }

    return usersList; // Возвращаем список пользователей
}
int MainWindow::GetSecond(int chatid, int firstuser) {
    QSqlQuery query(chatsdb); // Создаем запрос к базе данных

    // Подготавливаем SQL запрос для получения участников чата
    query.prepare("SELECT members FROM Chats WHERE ChatId = :chatid");
    query.bindValue(":chatid", chatid);

    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса";
        return -1; // Возвращаем -1 в случае ошибки
    }

    if (query.next()) {
        QString membersJson = query.value("members").toString(); // Получаем строку JSON с участниками
        QJsonDocument doc = QJsonDocument::fromJson(membersJson.toUtf8()); // Преобразуем строку в JSON документ
        QJsonArray membersArray = doc.array(); // Получаем массив участников

        // Ищем id первого пользователя в массиве
        for (const QJsonValue& member : membersArray) {
            int memberId = member.toInt();
            if (memberId != firstuser) {
                return memberId; // Возвращаем id второго участника
            }
        }
    }

    return -1; // Если второго участника не найдено, возвращаем -1
}
const QString& MainWindow::GetUserName(int id) {
    static QString userName; // Статическая переменная для хранения имени пользователя

    QSqlQuery query(userdb); // Создаем запрос к базе данных

    // Подготавливаем SQL запрос для получения имени пользователя по id
    query.prepare("SELECT Name FROM Users WHERE Id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса";
        return "none"; // Возвращаем пустую строку в случае ошибки
    }

    if (query.next()) {
        userName = query.value("Name").toString(); // Получаем имя пользователя
    } else {
        userName = ""; // Если пользователь не найден, устанавливаем пустую строку
    }

    return userName; // Возвращаем имя пользователя
}
QJsonArray MainWindow::GetMessages(int chatid) {
    QJsonArray messagesArray;
    QSqlQuery query(chatsdb);

    query.prepare("SELECT Messages FROM Chats WHERE ChatId = :chatid");
    query.bindValue(":chatid", chatid);

    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса";
        return messagesArray;
    }


    if (query.next()) {
        QString messagesJsonStr = query.value("Messages").toString();
        QJsonDocument doc = QJsonDocument::fromJson(messagesJsonStr.toUtf8());
        messagesArray = doc.array();
    }

    return messagesArray;
}
QList<QMap<QString, QVariant>> MainWindow::GetText(int mesid) {
    QList<QMap<QString, QVariant>> messagesList; // Список для хранения сообщений
    QSqlQuery query(messagesdb); // Создаем запрос к базе данных

    query.prepare("SELECT time, sender, writed FROM Messages WHERE MesId = :mesid");
    query.bindValue(":mesid", mesid);

    if (!query.exec()) {
        qDebug() << "Ошибка выполнения запроса";
        return messagesList; // Возвращаем пустой список в случае ошибки
    }

    // Шаг 2: Извлекаем данные
    if (query.next()) {
        QMap<QString, QVariant> messageMap; // Создаем карту для хранения данных сообщения
        messageMap["time"] = query.value("time").toString();    // Время отправления
        messageMap["sender"] = query.value("sender").toString(); // Отправитель
        messageMap["text"] = query.value("writed").toString();   // Текст сообщения

        messagesList.append(messageMap); // Добавляем карту в список
    }

    return messagesList; // Возвращаем список с сообщением
}
MainWindow::~MainWindow()
{
    delete ui;
}
