
#include <stdio.h>

int main() {
    int n, m, i, j, k = 0, found, count = 0;
    int a[10][10], mx[10][10], need[10][10];
    int avail[10], work[10], finish[10] = {0};
    int safe[10];

    printf("Enter number of processes and resources: ");
    scanf("%d%d", &n, &m);

    printf("Enter Allocation matrix:\n");
    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++)
            scanf("%d", &a[i][j]);

    printf("Enter Maximum matrix:\n");
    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++) {
            scanf("%d", &mx[i][j]);
            need[i][j] = mx[i][j] - a[i][j];
        }

    printf("Enter Available resources:\n");
    for (j = 0; j < m; j++) {
        scanf("%d", &avail[j]);
        work[j] = avail[j];
    }

    while (count < n) {
        found = 0;

        for (i = 0; i < n; i++) {
            if (!finish[i]) {
                for (j = 0; j < m; j++)
                    if (need[i][j] > work[j])
                        break;

                if (j == m) {
                    for (j = 0; j < m; j++)
                        work[j] += a[i][j];

                    safe[k++] = i;
                    finish[i] = 1;
                    count++;
                    found = 1;
                }
            }
        }

        if (!found)
            break;
    }

    if (count == n) {
        printf("\nSystem is SAFE\nSafe sequence: ");
        for (i = 0; i < n; i++)
            printf("P%d ", safe[i]);
    } else {
        printf("\nSystem is UNSAFE\n");
        printf("Processes unable to finish: ");

        for (i = 0; i < n; i++)
            if (!finish[i])
                printf("P%d ", i);
    }

    printf("\n");
    return 0;
}

