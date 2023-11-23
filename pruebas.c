#include <stdio.h>
#include <string.h>

// Función para comparar dos cadenas
int compararCadenas(const char *cadena1, const char *cadena2) {
    // Si ambas cadenas son nulas, son iguales
    if (cadena1 == NULL && cadena2 == NULL) {
        return 0;
    }

    // Si una cadena es nula y la otra no, son diferentes
    if (cadena1 == NULL || cadena2 == NULL) {
        return 1;
    }

    // Comparar las cadenas utilizando strcmp
    return strcmp(cadena1, cadena2);
}

int main() {
    char command[] = "solouncomando";
    char command_copy[sizeof(command)];  // Hacer una copia de la cadena original

    strcpy(command_copy, command);  // Copiar la cadena original

    char *token = strtok(command_copy, "|");

    // Usar la función compararCadenas en lugar de strcmp
    if (compararCadenas(command, token) == 0) {
        printf("Son iguales: %s\n", token);
    } else {
        printf("No son iguales.\n");
        printf("Token: %s\n", token);
        printf("Command: %s\n", command);
    }

    return 0;
}