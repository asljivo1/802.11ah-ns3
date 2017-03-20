// Generated by typings
// Source: https://raw.githubusercontent.com/DefinitelyTyped/DefinitelyTyped/56295f5058cac7ae458540423c50ac2dcf9fc711/sql.js/sql.js.d.ts
declare module SQL {

    class Database {
        constructor();
        constructor(data: Buffer);
        constructor(data: Uint8Array);
        constructor(data: number[]);

        run(sql: string): Database;
        run(sql: string, params: { [key: string]: number | string | Uint8Array }): Database;
        run(sql: string, params: (number | string | Uint8Array)[]): Database;

        exec(sql: string): QueryResults[];

        each(sql: string, callback: (obj: { [columnName: string]: number | string | Uint8Array }) => void, done: () => void): void;
        each(sql: string, params: { [key: string]: number | string | Uint8Array }, callback: (obj: { [columnName: string]: number | string | Uint8Array }) => void, done: () => void): void;
        each(sql: string, params: (number | string | Uint8Array)[], callback: (obj: { [columnName: string]: number | string | Uint8Array }) => void, done: () => void): void;

        prepare(sql: string): Statement;
        prepare(sql: string, params: { [key: string]: number | string | Uint8Array }): Statement;
        prepare(sql: string, params: (number | string | Uint8Array)[]): Statement;

        export(): Uint8Array;

        close(): void;
    }

    class Statement {
        bind(): boolean;
        bind(values: { [key: string]: number | string | Uint8Array }): boolean;
        bind(values: (number | string | Uint8Array)[]): boolean;

        step(): boolean;

        get(): (number | string | Uint8Array)[];
        get(params: { [key: string]: number | string | Uint8Array }): (number | string | Uint8Array)[];
        get(params: (number | string | Uint8Array)[]): (number | string | Uint8Array)[];

        getColumnNames(): string[];

        getAsObject(): { [columnName: string]: number | string | Uint8Array };
        getAsObject(params: { [key: string]: number | string | Uint8Array }): { [columnName: string]: number | string | Uint8Array };
        getAsObject(params: (number | string | Uint8Array)[]): { [columnName: string]: number | string | Uint8Array };

        run(): void;
        run(values: { [key: string]: number | string | Uint8Array }): void;
        run(values: (number | string | Uint8Array)[]): void;

        reset(): void;

        freemem(): void;

        free(): boolean;
    }

    interface QueryResults {
        columns: string[];
        values: (number | string | Uint8Array)[][];
    }

}